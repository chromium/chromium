// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/remote_font_face_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
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

class IftCustomFontData::PatchLoader final
    : public GarbageCollected<PatchLoader>,
      public RawResourceClient {
 public:
  PatchLoader(const IftCustomFontData* owner, String patch_url)
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

  String DebugName() const override { return "IftCustomFontData::PatchLoader"; }

 private:
  Member<const IftCustomFontData> owner_;
  const String patch_url_;
};

IftCustomFontData* IftCustomFontData::MaybeCreate(
    RemoteFontFaceSource* font_face_source,
    FontResource& font_resource) {
  std::unique_ptr<IftPatcher> patcher = font_resource.TakeIftPatcher();
  if (!patcher) {
    return nullptr;
  }
  // The "initial font URL" from the IFT spec. Patch URLs are resolved against
  // it and it is sent as the referrer of patch requests. The spec does not say
  // whether this is the URL before or after redirects. Use the URL after
  // redirects, matching how Blink handles URLs within stylesheets, so that
  // patches are fetched relative to where the font bytes actually came from.
  const KURL initial_font_url = font_resource.GetResponse().ResponseUrl();
  return MakeGarbageCollected<IftCustomFontData>(
      base::PassKey<IftCustomFontData>(), font_face_source, initial_font_url,
      font_resource.Options().WorldForCsp(), std::move(patcher));
}

IftCustomFontData::IftCustomFontData(base::PassKey<IftCustomFontData>,
                                     RemoteFontFaceSource* font_face_source,
                                     const KURL& font_url,
                                     const DOMWrapperWorld* world_for_csp,
                                     std::unique_ptr<IftPatcher> patcher)
    : font_face_source_(font_face_source),
      font_url_(font_url),
      world_for_csp_(world_for_csp),
      patcher_(std::move(patcher)),
      requested_subset_(std::make_unique<IftSubsetDefinition>()),
      fetched_subset_(std::make_unique<IftSubsetDefinition>()),
      scratch_subset_(std::make_unique<IftSubsetDefinition>()) {
  CHECK(font_face_source_);
  CHECK(patcher_);
}

IftCustomFontData::~IftCustomFontData() = default;

void IftCustomFontData::Trace(Visitor* visitor) const {
  visitor->Trace(font_face_source_);
  visitor->Trace(world_for_csp_);
  visitor->Trace(patch_loaders_);
  CustomFontData::Trace(visitor);
}

bool IftCustomFontData::IftRequireSubset(const StringView& text) const {
  if (state_ == State::kFailed) {
    return false;
  }
  scratch_subset_->Clear();
  for (UChar32 code_point : text) {
    scratch_subset_->AddCodepoint(static_cast<uint32_t>(code_point));
  }
  // If no patches intersect (either because the subset is already supported or
  // because the font has no patches for it), the font is ready to shape with
  // and any missing glyphs will fall back on `.notdef`.
  if (!patcher_->RenderedFontRequiresPatches(*scratch_subset_)) {
    return true;
  }
  requested_subset_->Union(*scratch_subset_);
  BeginCollecting();
  return false;
}

base::span<const uint8_t> IftCustomFontData::GetFontDataForTesting() const {
  return patcher_->GetFontData();
}

void IftCustomFontData::BeginIdle() const {
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
  font_face_source_->UpdateCustomFontData(custom_font_data);
}

void IftCustomFontData::BeginCollecting() const {
  if (state_ != State::kIdle) {
    return;
  }
  state_ = State::kCollecting;
  ExecutionContext* execution_context =
      font_face_source_->GetExecutionContext();
  if (!execution_context) {
    BeginFailed();
    return;
  }
  execution_context->GetTaskRunner(TaskType::kFontLoading)
      ->PostTask(FROM_HERE, BindOnce(&IftCustomFontData::BeginFetching,
                                     WrapPersistent(this)));
}

void IftCustomFontData::BeginFetching() const {
  if (state_ != State::kCollecting) {
    return;
  }
  ExecutionContext* execution_context =
      font_face_source_->GetExecutionContext();
  ResourceFetcher* fetcher =
      execution_context ? execution_context->Fetcher() : nullptr;
  if (!fetcher) {
    BeginFailed();
    return;
  }

  state_ = State::kFetching;
  fetched_subset_->Union(*requested_subset_);
  Vector<String> patch_urls = patcher_->RequestPatches(*fetched_subset_);
  if (patch_urls.empty()) {
    BeginIdle();
    return;
  }

  // Register every loader before starting any fetch so that a fetch finishing
  // synchronously does not observe an incomplete `patch_loaders_`.
  for (const String& patch_url : patch_urls) {
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
    params.SetCrossOriginAccessControl(execution_context->GetSecurityOrigin(),
                                       kCrossOriginAttributeAnonymous);
    RawResource::Fetch(params, fetcher, loader);
  }
}

void IftCustomFontData::ApplyPatches() const {
  if (state_ != State::kFetching) {
    return;
  }
  // TODO(wmedrano): Consider limiting the max number of patch applications. At
  // some point, we may want to just request the entire subset.
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
      BeginFailed();
      return;
  }
}

void IftCustomFontData::BeginFailed() const {
  if (state_ == State::kFailed) {
    return;
  }
  state_ = State::kFailed;
}

void IftCustomFontData::OnPatchLoaded(PatchLoader* loader,
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

}  // namespace blink
