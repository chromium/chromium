// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "base/types/expected.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class IftFont::PatchLoader final : public GarbageCollected<PatchLoader>,
                                   public RawResourceClient {
 public:
  PatchLoader(const IftFont* owner, String patch_url)
      : owner_(owner), patch_url_(std::move(patch_url)) {
    CHECK(owner_);
  }

  // The patch URL as returned by `IftPatcher::RequestPatches()`, before being
  // resolved against the font URL.
  const String& PatchUrl() const { return patch_url_; }

  void Detach() { ClearResource(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(owner_);
    RawResourceClient::Trace(visitor);
  }

  void NotifyFinished(Resource* resource) override {
    owner_->OnPatchLoaded(this, resource);
  }

  String DebugName() const override { return "IftFont::PatchLoader"; }

 private:
  Member<const IftFont> owner_;
  const String patch_url_;
};

IftFont* IftFont::MaybeCreate(IftFontObserver* observer,
                              FontResource& font_resource) {
  std::unique_ptr<IftPatcher> patcher = font_resource.CloneIftPatcher();
  if (!patcher) {
    return nullptr;
  }
  // The "initial font URL" from the IFT spec. Patch URLs are resolved against
  // it and it is sent as the referrer of patch requests. The spec does not say
  // whether this is the URL before or after redirects. Use the URL after
  // redirects, matching how Blink handles URLs within stylesheets, so that
  // patches are fetched relative to where the font bytes actually came from.
  const KURL initial_font_url = font_resource.GetResponse().ResponseUrl();
  return MakeGarbageCollected<IftFont>(
      base::PassKey<IftFont>(), observer, initial_font_url,
      font_resource.Options().WorldForCsp(), std::move(patcher));
}

IftFont::IftFont(base::PassKey<IftFont>,
                 IftFontObserver* observer,
                 const KURL& font_url,
                 const DOMWrapperWorld* world_for_csp,
                 std::unique_ptr<IftPatcher> patcher)
    : observer_(observer),
      font_url_(font_url),
      world_for_csp_(world_for_csp),
      patcher_(std::move(patcher)),
      requested_subset_(std::make_unique<IftSubsetDefinition>()),
      fetched_subset_(std::make_unique<IftSubsetDefinition>()),
      scratch_subset_(std::make_unique<IftSubsetDefinition>()) {
  CHECK(observer_);
  CHECK(patcher_);
}

IftFont::~IftFont() = default;

void IftFont::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(world_for_csp_);
  visitor->Trace(patch_loaders_);
}

bool IftFont::RequireSubset(
    const StringView& text,
    base::span<const FontVariationAxis> variation_axes) const {
  if (state_ == State::kFailed) {
    return false;
  }
  if (text.empty()) {
    return true;
  }
  scratch_subset_->Clear();
  for (UChar32 code_point : text) {
    scratch_subset_->AddCodepoint(static_cast<uint32_t>(code_point));
  }
  for (const FontVariationAxis& axis : variation_axes) {
    scratch_subset_->AddDesignSpace(axis.Tag(), axis.Value(), axis.Value());
  }
  // If there are no patches either the font already supports the subset, or the
  // font can never support the subset. In the no support case, it is up to the
  // shaper to handle fallbacks.
  if (!patcher_->RenderedFontRequiresPatches(*scratch_subset_)) {
    return true;
  }
  requested_subset_->Union(*scratch_subset_);
  BeginCollecting();
  return false;
}

base::span<const uint8_t> IftFont::GetFontDataForTesting() const {
  return patcher_->GetFontData();
}

void IftFont::SetPatchLimitsForTesting(uint32_t max_invalidating_patches,
                                       uint32_t max_total_patches) const {
  patcher_->SetPatchLimitsForTesting(max_invalidating_patches,
                                     max_total_patches);
}

void IftFont::BeginIdle() const {
  if (state_ == State::kFailed) {
    return;
  }
  state_ = State::kIdle;
  // Nothing was patched since the font was last rendered, so there is no text
  // waiting on an invalidation.
  if (!patcher_->HasUnrenderedChanges()) {
    return;
  }
  scoped_refptr<SharedBuffer> buffer =
      SharedBuffer::Create(patcher_->GetFontData());
  String ots_parse_message;
  FontCustomPlatformData* custom_font_data =
      FontCustomPlatformData::Create(buffer.get(), ots_parse_message);
  if (!custom_font_data) {
    BeginFailed();
    return;
  }
  patcher_->MarkFontDataRendered();
  observer_->OnIftFontUpdated(custom_font_data);
}

void IftFont::BeginCollecting() const {
  if (state_ != State::kIdle) {
    return;
  }
  state_ = State::kCollecting;
  // Leaving `kIdle` starts a new execution of the extension algorithm. Subsets
  // requested before returning to `kIdle` share its patch limits.
  patcher_->BeginExtension();
  ResourceFetcher* fetcher = observer_->GetIftResourceFetcher();
  if (!fetcher || fetcher->IsDetached()) {
    BeginFailed();
    return;
  }
  fetcher->GetTaskRunner()->PostTask(
      FROM_HERE, BindOnce(&IftFont::BeginFetching, WrapPersistent(this)));
}

void IftFont::BeginFetching() const {
  if (state_ != State::kCollecting) {
    return;
  }
  ResourceFetcher* fetcher = observer_->GetIftResourceFetcher();
  if (!fetcher || fetcher->IsDetached()) {
    BeginFailed();
    return;
  }

  state_ = State::kFetching;
  fetched_subset_->Union(*requested_subset_);
  base::expected<Vector<String>, IftPatcher::RequestError> patch_urls =
      patcher_->RequestPatches(*fetched_subset_);
  if (!patch_urls.has_value()) {
    BeginFailed();
    return;
  }
  if (patch_urls->empty()) {
    BeginIdle();
    return;
  }

  const SecurityOrigin* security_origin = fetcher->GetProperties()
                                              .GetFetchClientSettingsObject()
                                              .GetSecurityOrigin();
  // Register every loader before starting any fetch so that a fetch finishing
  // synchronously does not observe an incomplete `patch_loaders_`.
  for (const String& patch_url : *patch_urls) {
    PatchLoader* loader = MakeGarbageCollected<PatchLoader>(this, patch_url);
    patch_loaders_.insert(loader);
    // https://www.w3.org/TR/IFT/#abstract-opdef-load-patch-file
    ResourceRequest resource_request(KURL(font_url_, patch_url));
    resource_request.SetRequestContext(mojom::blink::RequestContextType::FONT);
    resource_request.SetRequestDestination(
        network::mojom::RequestDestination::kFont);
    resource_request.SetReferrerString(font_url_.GetString());
    ResourceLoaderOptions options(world_for_csp_);
    options.initiator_info.name = fetch_initiator_type_names::kFont;
    FetchParameters params(std::move(resource_request), std::move(options));
    params.SetCrossOriginAccessControl(security_origin,
                                       kCrossOriginAttributeAnonymous);
    RawResource::Fetch(params, fetcher, loader);
  }
}

void IftFont::ApplyPatches() const {
  if (state_ != State::kFetching) {
    return;
  }
  switch (patcher_->ApplyPatches(*fetched_subset_)) {
    case IftPatcher::ApplyStatus::kSuccess:
    case IftPatcher::ApplyStatus::kMissingPatches:
      // Attempt to refetch in case applying an invalidating patch revealed
      // additional patches needed for `fetched_subset_`, or in case
      // `requested_subset_` was extended while fetching.
      state_ = State::kCollecting;
      BeginFetching();
      return;
    case IftPatcher::ApplyStatus::kInvalidFont:
    case IftPatcher::ApplyStatus::kPatchGroupError:
    case IftPatcher::ApplyStatus::kPatchError:
    case IftPatcher::ApplyStatus::kPatchLimitExceeded:
      BeginFailed();
      return;
  }
}

void IftFont::BeginFailed() const {
  if (state_ == State::kFailed) {
    return;
  }
  state_ = State::kFailed;
}

void IftFont::OnPatchLoaded(PatchLoader* loader,
                            const Resource* resource) const {
  patch_loaders_.erase(loader);
  loader->Detach();
  if (state_ != State::kFetching) {
    return;
  }
  const ResourceResponse& response = resource->GetResponse();
  if (resource->ErrorOccurred() ||
      (response.IsHTTP() &&
       !network::IsSuccessfulStatus(response.HttpStatusCode()))) {
    BeginFailed();
    return;
  }
  scoped_refptr<const SharedBuffer> data = resource->ResourceBuffer();
  patcher_->AddPatchData(
      loader->PatchUrl(),
      data ? data->CopyAs<Vector<uint8_t>>() : Vector<uint8_t>());
  if (patch_loaders_.empty()) {
    ApplyPatches();
  }
}

IftCustomFontData* IftCustomFontData::Create(
    const IftFont* ift_font,
    const FontCustomPlatformData& font_platform_data,
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  // Resolve variation axis coordinates without clamping or filtering against
  // the base font's `fvar` table, which in an unpatched IFT font may omit
  // pinned axes or have narrowed `[min, max]` ranges until patches are applied.
  // Coordinates for axes the font does not have never match a patch; see
  // https://www.w3.org/TR/IFT/#abstract-opdef-check-entry-intersection.
  //
  // TODO(wmedrano): Clamp out-of-bounds CSS coordinates (e.g. `wght: 950`,
  // `slnt: -14`) to the full font / patch-map axis bounds so finite outer patch
  // segments still match, and include `fvar` defaults for omitted axes
  // (`font-optical-sizing: none` or custom axes) when needed.
  return MakeGarbageCollected<IftCustomFontData>(
      ift_font,
      font_platform_data.ResolveVariationAxes(
          font_description, font_selection_capabilities,
          FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes));
}

IftCustomFontData::IftCustomFontData(
    const IftFont* ift_font,
    Vector<FontVariationAxis, 5> variation_axes)
    : ift_font_(ift_font), variation_axes_(std::move(variation_axes)) {
  CHECK(ift_font_);
}

IftCustomFontData::~IftCustomFontData() = default;

void IftCustomFontData::Trace(Visitor* visitor) const {
  visitor->Trace(ift_font_);
  CustomFontData::Trace(visitor);
}

bool IftCustomFontData::IftRequireSubset(const StringView& text) const {
  return ift_font_->RequireSubset(text, variation_axes_);
}

}  // namespace blink
