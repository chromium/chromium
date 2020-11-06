// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"

#include "skia/public/mojom/skcolor.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"

namespace blink {

void FakeLocalFrameHost::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      mojom::blink::LocalFrameHost::Name_,
      base::BindRepeating(&FakeLocalFrameHost::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeLocalFrameHost::EnterFullscreen(
    mojom::blink::FullscreenOptionsPtr options,
    EnterFullscreenCallback callback) {
  std::move(callback).Run(true);
}

void FakeLocalFrameHost::ExitFullscreen() {}

void FakeLocalFrameHost::FullscreenStateChanged(bool is_fullscreen) {}

void FakeLocalFrameHost::RegisterProtocolHandler(const WTF::String& scheme,
                                                 const ::blink::KURL& url,
                                                 bool user_gesture) {}

void FakeLocalFrameHost::UnregisterProtocolHandler(const WTF::String& scheme,
                                                   const ::blink::KURL& url,
                                                   bool user_gesture) {}

void FakeLocalFrameHost::DidDisplayInsecureContent() {}

void FakeLocalFrameHost::DidAddContentSecurityPolicies(
    WTF::Vector<::network::mojom::blink::ContentSecurityPolicyPtr>) {}
void FakeLocalFrameHost::DidContainInsecureFormAction() {}

void FakeLocalFrameHost::DocumentAvailableInMainFrame(
    bool uses_temporary_zoom_level) {}

void FakeLocalFrameHost::SetNeedsOcclusionTracking(bool needs_tracking) {}
void FakeLocalFrameHost::SetVirtualKeyboardOverlayPolicy(
    bool vk_overlays_content) {}

void FakeLocalFrameHost::VisibilityChanged(
    mojom::blink::FrameVisibility visibility) {}

void FakeLocalFrameHost::DidChangeThemeColor(
    base::Optional<::SkColor> theme_color) {}

void FakeLocalFrameHost::DidChangeBackgroundColor(SkColor background_color,
                                                  bool color_adjust) {}

void FakeLocalFrameHost::DidFailLoadWithError(const ::blink::KURL& url,
                                              int32_t error_code) {}

void FakeLocalFrameHost::DidFocusFrame() {}

void FakeLocalFrameHost::DidCallFocus() {}

void FakeLocalFrameHost::EnforceInsecureRequestPolicy(
    mojom::InsecureRequestPolicy policy_bitmap) {}

void FakeLocalFrameHost::EnforceInsecureNavigationsSet(
    const WTF::Vector<uint32_t>& set) {}

void FakeLocalFrameHost::DidChangeActiveSchedulerTrackedFeatures(
    uint64_t features_mask) {}

void FakeLocalFrameHost::SuddenTerminationDisablerChanged(
    bool present,
    blink::mojom::SuddenTerminationDisablerType disabler_type) {}

void FakeLocalFrameHost::HadStickyUserActivationBeforeNavigationChanged(
    bool value) {}

void FakeLocalFrameHost::ScrollRectToVisibleInParentFrame(
    const gfx::Rect& rect_to_scroll,
    blink::mojom::blink::ScrollIntoViewParamsPtr params) {}

void FakeLocalFrameHost::BubbleLogicalScrollInParentFrame(
    blink::mojom::blink::ScrollDirection direction,
    ui::ScrollGranularity granularity) {}

void FakeLocalFrameHost::DidAccessInitialDocument() {}

void FakeLocalFrameHost::DidBlockNavigation(
    const KURL& blocked_url,
    const KURL& initiator_url,
    mojom::NavigationBlockedReason reason) {}

void FakeLocalFrameHost::DidChangeLoadProgress(double load_progress) {}

void FakeLocalFrameHost::DidFinishLoad(const KURL& validated_url) {}

void FakeLocalFrameHost::DispatchLoad() {}

void FakeLocalFrameHost::GoToEntryAtOffset(int32_t offset,
                                           bool has_user_gesture) {}

void FakeLocalFrameHost::RenderFallbackContentInParentProcess() {}

void FakeLocalFrameHost::UpdateTitle(
    const WTF::String& title,
    base::i18n::TextDirection title_direction) {}

void FakeLocalFrameHost::UpdateUserActivationState(
    mojom::blink::UserActivationUpdateType update_type,
    mojom::UserActivationNotificationType notification_type) {}

void FakeLocalFrameHost::HandleAccessibilityFindInPageResult(
    mojom::blink::FindInPageResultAXParamsPtr params) {}

void FakeLocalFrameHost::HandleAccessibilityFindInPageTermination() {}

void FakeLocalFrameHost::DocumentOnLoadCompleted() {}

void FakeLocalFrameHost::ForwardResourceTimingToParent(
    mojom::blink::ResourceTimingInfoPtr timing) {}

void FakeLocalFrameHost::DidFinishDocumentLoad() {}

void FakeLocalFrameHost::RunModalAlertDialog(
    const WTF::String& alert_message,
    RunModalAlertDialogCallback callback) {
  std::move(callback).Run();
}

void FakeLocalFrameHost::RunModalConfirmDialog(
    const WTF::String& alert_message,
    RunModalConfirmDialogCallback callback) {
  std::move(callback).Run(true);
}

void FakeLocalFrameHost::RunModalPromptDialog(
    const WTF::String& alert_message,
    const WTF::String& default_value,
    RunModalPromptDialogCallback callback) {
  std::move(callback).Run(true, g_empty_string);
}

void FakeLocalFrameHost::RunBeforeUnloadConfirm(
    bool is_reload,
    RunBeforeUnloadConfirmCallback callback) {
  std::move(callback).Run(true);
}

void FakeLocalFrameHost::UpdateFaviconURL(
    WTF::Vector<blink::mojom::blink::FaviconURLPtr> favicon_urls) {}

void FakeLocalFrameHost::DownloadURL(
    mojom::blink::DownloadURLParamsPtr params) {}

void FakeLocalFrameHost::FocusedElementChanged(
    bool is_editable_element,
    const gfx::Rect& bounds_in_frame_widget,
    blink::mojom::FocusType focus_type) {}

void FakeLocalFrameHost::TextSelectionChanged(const WTF::String& text,
                                              uint32_t offset,
                                              const gfx::Range& range) {}
void FakeLocalFrameHost::ShowPopupMenu(
    mojo::PendingRemote<mojom::blink::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    Vector<mojom::blink::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {}

void FakeLocalFrameHost::DidLoadResourceFromMemoryCache(
    const KURL& url,
    const WTF::String& http_method,
    const WTF::String& mime_type,
    network::mojom::blink::RequestDestination request_destination) {}

void FakeLocalFrameHost::DidChangeFrameOwnerProperties(
    const base::UnguessableToken& child_frame_token,
    mojom::blink::FrameOwnerPropertiesPtr frame_owner_properties) {}

void FakeLocalFrameHost::DidChangeOpener(
    const base::Optional<base::UnguessableToken>& opener_frame) {}

void FakeLocalFrameHost::DidChangeCSPAttribute(
    const base::UnguessableToken& child_frame_token,
    network::mojom::blink::ContentSecurityPolicyPtr) {}

void FakeLocalFrameHost::DidChangeFramePolicy(
    const base::UnguessableToken& child_frame_token,
    const FramePolicy& frame_policy) {}

void FakeLocalFrameHost::BindPolicyContainer(
    mojo::PendingAssociatedReceiver<mojom::blink::PolicyContainerHost>
        receiver) {}

void FakeLocalFrameHost::CapturePaintPreviewOfSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {}

void FakeLocalFrameHost::Detach() {}

void FakeLocalFrameHost::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::blink::LocalFrameHost>(
      std::move(handle)));
}

}  // namespace blink
