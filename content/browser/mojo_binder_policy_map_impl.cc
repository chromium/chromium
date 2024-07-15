// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_map_impl.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "content/common/dom_automation_controller.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/mojo_binder_policy_map.h"
#include "content/public/common/content_client.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom.h"
#if BUILDFLAG(IS_MAC)
#include "third_party/blink/public/mojom/input/text_input_host.mojom.h"
#endif
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_observer.mojom.h"
#include "third_party/blink/public/mojom/media/renderer_audio_output_stream_factory.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"

namespace content {

#if BUILDFLAG(IS_MAC)
// Put crbug.com/115920 fix under flag, so we can measure its CWV impact.
BASE_FEATURE(kTextInputHostMojoCapabilityControlWorkaround,
             "TextInputHostMojoCapabilityControlWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

namespace {

enum class PolicyClass {
  kSameOriginPrerendering,
  kPreview,
};

// Register feature specific policies for interfaces registered in
// `internal::PopulateBinderMap` and `internal::PopulateBinderMapWithContext`.
void RegisterNonAssociatedPolicies(MojoBinderPolicyMap& map,
                                   PolicyClass policy) {
  // For Prerendering, kCancel is usually used for those interfaces that cannot
  // be granted because they can cause undesirable side-effects (e.g., playing
  // audio, showing notification) and are non-deferrable.
  // Please update `PrerenderCancelledInterface` and
  // `GetCancelledInterfaceType()` in
  // content/browser/preloading/prerender/prerender_metrics.h once you add a new
  // kCancel interface.

  map.SetNonAssociatedPolicy<device::mojom::GamepadHapticsManager>(
      MojoBinderNonAssociatedPolicy::kCancel);
  map.SetNonAssociatedPolicy<device::mojom::GamepadMonitor>(
      MojoBinderNonAssociatedPolicy::kCancel);

  if (policy == PolicyClass::kSameOriginPrerendering) {
    // ClipboardHost has sync messages, so it cannot be kDefer. However, the
    // renderer is not expected to request the interface; prerendering documents
    // do not have system focus nor user activation, which is required before
    // sending the request.
    map.SetNonAssociatedPolicy<blink::mojom::ClipboardHost>(
        MojoBinderNonAssociatedPolicy::kUnexpected);
  }

  // FileUtilitiesHost is only used by APIs that require user activations, being
  // impossible for a prerendered document. For the reason, this is marked as
  // kUnexpected.
  map.SetNonAssociatedPolicy<blink::mojom::FileUtilitiesHost>(
      MojoBinderNonAssociatedPolicy::kUnexpected);

  map.SetNonAssociatedPolicy<blink::mojom::CacheStorage>(
      MojoBinderNonAssociatedPolicy::kGrant);
  map.SetNonAssociatedPolicy<blink::mojom::IDBFactory>(
      MojoBinderNonAssociatedPolicy::kGrant);

  // Grant this interface because some sync web APIs rely on it; deferring it
  // leads to deadlock. However, granting this interface does not mean that
  // prerenders are allowed to create output streams.
  // RenderFrameAudioOutputStreamFactory understands which pages are
  // prerendering and does not fulfill their requests for audio streams.
  map.SetNonAssociatedPolicy<blink::mojom::RendererAudioOutputStreamFactory>(
      MojoBinderNonAssociatedPolicy::kGrant);
  map.SetNonAssociatedPolicy<network::mojom::RestrictedCookieManager>(
      MojoBinderNonAssociatedPolicy::kGrant);
  // Set policy to Grant for CodeCacheHost. Without this loads won't progress
  // since we wait for a response from code cache when loading resources.
  map.SetNonAssociatedPolicy<blink::mojom::CodeCacheHost>(
      MojoBinderNonAssociatedPolicy::kGrant);

  // Grant this for Media Capabilities APIs. This should be safe as the APIs
  // just query encoding / decoding information.
  map.SetNonAssociatedPolicy<media::mojom::WebrtcVideoPerfHistory>(
      content::MojoBinderNonAssociatedPolicy::kGrant);

#if BUILDFLAG(IS_MAC)
  // Set policy to Grant for TextInputHost.
  // This is used to return macOS IME sync call results to the browser process,
  // and will hang entire Chrome if paused.
  // This is a prospective fix added for crbug.com/1480850
  if (base::FeatureList::IsEnabled(
          kTextInputHostMojoCapabilityControlWorkaround)) {
    map.SetNonAssociatedPolicy<blink::mojom::TextInputHost>(
        MojoBinderNonAssociatedPolicy::kGrant);
  }
#endif
}

// Register same-origin prerendering policies for channel-associated interfaces
// registered in `RenderFrameHostImpl::SetUpMojoIfNeeded()`.
void RegisterChannelAssociatedPoliciesForSameOriginPrerendering(
    MojoBinderPolicyMap& map) {
  // Basic skeleton. All of them are critical to load a page so their policies
  // have to be kGrant.
  // TODO(crbug.com/40201285): Message-level control should be performed.
  map.SetAssociatedPolicy<mojom::FrameHost>(MojoBinderAssociatedPolicy::kGrant);
  map.SetAssociatedPolicy<blink::mojom::LocalFrameHost>(
      MojoBinderAssociatedPolicy::kGrant);
  map.SetAssociatedPolicy<blink::mojom::LocalMainFrameHost>(
      MojoBinderAssociatedPolicy::kGrant);

  // These interfaces do not leak sensitive information.
  map.SetAssociatedPolicy<blink::mojom::BackForwardCacheControllerHost>(
      MojoBinderAssociatedPolicy::kGrant);
  map.SetAssociatedPolicy<blink::mojom::ManifestUrlChangeObserver>(
      MojoBinderAssociatedPolicy::kGrant);
  map.SetAssociatedPolicy<mojom::DomAutomationControllerHost>(
      MojoBinderAssociatedPolicy::kGrant);

  // BroadcastChannel is granted for prerendering, as this API is restricted to
  // same-origin.
  map.SetAssociatedPolicy<blink::mojom::BroadcastChannelProvider>(
      MojoBinderAssociatedPolicy::kGrant);

  // Granting this interface does not mean prerendering pages are allowed to
  // play media. Feature-specific capability control is implemented to delay
  // playing media. See `RenderFrameImpl::DeferMediaLoad` for more information.
  map.SetAssociatedPolicy<media::mojom::MediaPlayerHost>(
      MojoBinderAssociatedPolicy::kGrant);

  // DisplayCutout supports the CSS viewport-fit property. It tracks
  // the current viewport-fit on a per-document basis, but only calls
  // the WebContents::NotifyViewportFitChanged and informs WebContents's
  // observers when the document is fullscreened. Prerendered documents cannot
  // enter fullscreen because they do not have transient activation, nor are
  // they active documents (see RenderFrameHostImpl::EnterFullscreen), so it is
  // safe to allow a prerendered document to use it.
  map.SetAssociatedPolicy<blink::mojom::DisplayCutoutHost>(
      MojoBinderAssociatedPolicy::kGrant);

  // Prerendering pages are allowed to create urls for blobs.
  map.SetAssociatedPolicy<blink::mojom::BlobURLStore>(
      MojoBinderAssociatedPolicy::kGrant);

  // Pages with FetchLater API calls should be allowed to prerender.
  // TODO(crbug.com/40276121): Update according to feedback from
  // https://github.com/WICG/pending-beacon/issues/82
  map.SetAssociatedPolicy<blink::mojom::FetchLaterLoaderFactory>(
      MojoBinderAssociatedPolicy::kGrant);
}

// Register mojo binder policies for same-origin prerendering for content/
// interfaces.
void RegisterContentBinderPoliciesForSameOriginPrerendering(
    MojoBinderPolicyMap& map) {
  RegisterNonAssociatedPolicies(map, PolicyClass::kSameOriginPrerendering);
  RegisterChannelAssociatedPoliciesForSameOriginPrerendering(map);
}

// Register mojo binder policies for preview mode for content/ interfaces.
void RegisterContentBinderPoliciesForPreview(MojoBinderPolicyMap& map) {
  RegisterNonAssociatedPolicies(map, PolicyClass::kPreview);
  // Inherits the policies for same-origin prerendering.
  // TODO(b:299240273): Adjust policies for preview.
  RegisterChannelAssociatedPoliciesForSameOriginPrerendering(map);
}

// A singleton class that stores the `MojoBinderPolicyMap` of interfaces which
// are obtained via `BrowserInterfaceBrowser` for frames.
// content/ initializes the policy map with predefined policies, then allows
// embedders to update the map.
class BrowserInterfaceBrokerMojoBinderPolicyMapHolder {
 public:
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder() {
    RegisterContentBinderPoliciesForSameOriginPrerendering(same_origin_map_);
    GetContentClient()
        ->browser()
        ->RegisterMojoBinderPoliciesForSameOriginPrerendering(same_origin_map_);

    RegisterContentBinderPoliciesForPreview(preview_map_);
    GetContentClient()->browser()->RegisterMojoBinderPoliciesForPreview(
        preview_map_);
  }

  ~BrowserInterfaceBrokerMojoBinderPolicyMapHolder() = default;

  // Remove copy and move operations.
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder(
      const BrowserInterfaceBrokerMojoBinderPolicyMapHolder& other) = delete;
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder& operator=(
      const BrowserInterfaceBrokerMojoBinderPolicyMapHolder& other) = delete;
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder(
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder&&) = delete;
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder& operator=(
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder&&) = delete;

  const MojoBinderPolicyMapImpl* GetSameOriginPolicyMap() const {
    return &same_origin_map_;
  }
  const MojoBinderPolicyMapImpl* GetPreviewPolicyMap() const {
    return &preview_map_;
  }

 private:
  // TODO(crbug.com/40156088): Set default policy map for content/.
  // Changes to `same_origin_map_` require security review.
  MojoBinderPolicyMapImpl same_origin_map_;

  MojoBinderPolicyMapImpl preview_map_;
};

}  // namespace

MojoBinderPolicyMapImpl::MojoBinderPolicyMapImpl() = default;

MojoBinderPolicyMapImpl::MojoBinderPolicyMapImpl(
    const base::flat_map<std::string, MojoBinderNonAssociatedPolicy>& init_map)
    : non_associated_policy_map_(init_map) {}

MojoBinderPolicyMapImpl::~MojoBinderPolicyMapImpl() = default;

const MojoBinderPolicyMapImpl*
MojoBinderPolicyMapImpl::GetInstanceForSameOriginPrerendering() {
  static const base::NoDestructor<
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder>
      map;

  return map->GetSameOriginPolicyMap();
}

const MojoBinderPolicyMapImpl*
MojoBinderPolicyMapImpl::GetInstanceForPreview() {
  static const base::NoDestructor<
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder>
      map;

  return map->GetPreviewPolicyMap();
}

MojoBinderNonAssociatedPolicy
MojoBinderPolicyMapImpl::GetNonAssociatedMojoBinderPolicy(
    const std::string& interface_name,
    const MojoBinderNonAssociatedPolicy default_policy) const {
  const auto& found = non_associated_policy_map_.find(interface_name);
  if (found != non_associated_policy_map_.end())
    return found->second;
  return default_policy;
}

MojoBinderAssociatedPolicy
MojoBinderPolicyMapImpl::GetAssociatedMojoBinderPolicy(
    const std::string& interface_name,
    const MojoBinderAssociatedPolicy default_policy) const {
  const auto& found = associated_policy_map_.find(interface_name);
  if (found != associated_policy_map_.end())
    return found->second;
  return default_policy;
}

MojoBinderNonAssociatedPolicy
MojoBinderPolicyMapImpl::GetNonAssociatedMojoBinderPolicyOrDieForTesting(
    const std::string& interface_name) const {
  const auto& found = non_associated_policy_map_.find(interface_name);
  CHECK(found != non_associated_policy_map_.end(), base::NotFatalUntil::M130);
  return found->second;
}

MojoBinderAssociatedPolicy
MojoBinderPolicyMapImpl::GetAssociatedMojoBinderPolicyOrDieForTesting(
    const std::string& interface_name) const {
  const auto& found = associated_policy_map_.find(interface_name);
  CHECK(found != associated_policy_map_.end(), base::NotFatalUntil::M130);
  return found->second;
}

void MojoBinderPolicyMapImpl::SetPolicyByName(
    const std::string_view& name,
    MojoBinderNonAssociatedPolicy policy) {
  non_associated_policy_map_.emplace(name, policy);
}

void MojoBinderPolicyMapImpl::SetPolicyByName(
    const std::string_view& name,
    MojoBinderAssociatedPolicy policy) {
  associated_policy_map_.emplace(name, policy);
}

}  // namespace content
