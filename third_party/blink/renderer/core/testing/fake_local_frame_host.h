// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FAKE_LOCAL_FRAME_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FAKE_LOCAL_FRAME_HOST_H_

#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"

namespace blink {

// This class implements a LocalFrameHost that can be attached to the
// AssociatedInterfaceProvider so that it will be called when the renderer
// normally sends a request to the browser process. But for a unittest
// setup it can be intercepted by this class.
class FakeLocalFrameHost : public mojom::blink::LocalFrameHost {
 public:
  FakeLocalFrameHost() = default;

  void Init(blink::AssociatedInterfaceProvider* provider);
  void EnterFullscreen(mojom::blink::FullscreenOptionsPtr options,
                       EnterFullscreenCallback callback) override;
  void ExitFullscreen() override;
  void FullscreenStateChanged(bool is_fullscreen) override;
  void RegisterProtocolHandler(const WTF::String& scheme,
                               const ::blink::KURL& url,
                               const ::WTF::String& title,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(const WTF::String& scheme,
                                 const ::blink::KURL& url,
                                 bool user_gesture) override;
  void DidDisplayInsecureContent() override;
  void DidAddContentSecurityPolicies(
      WTF::Vector<::network::mojom::blink::ContentSecurityPolicyPtr>) override;
  void DidContainInsecureFormAction() override;
  void DocumentAvailableInMainFrame(bool uses_temporary_zoom_level) override;
  void SetNeedsOcclusionTracking(bool needs_tracking) override;
  void SetVirtualKeyboardOverlayPolicy(bool vk_overlays_content) override;
  void VisibilityChanged(mojom::blink::FrameVisibility visibility) override;
  void DidChangeThemeColor(
      const base::Optional<::SkColor>& theme_color) override;
  void DidChangeBackgroundColor(const SkColor& background_color) override;
  void DidFailLoadWithError(const ::blink::KURL& url,
                            int32_t error_code) override;
  void DidFocusFrame() override;
  void DidCallFocus() override;
  void EnforceInsecureRequestPolicy(
      mojom::InsecureRequestPolicy policy_bitmap) override;
  void EnforceInsecureNavigationsSet(const WTF::Vector<uint32_t>& set) override;
  void DidChangeActiveSchedulerTrackedFeatures(uint64_t features_mask) override;
  void SuddenTerminationDisablerChanged(
      bool present,
      blink::mojom::SuddenTerminationDisablerType disabler_type) override;
  void HadStickyUserActivationBeforeNavigationChanged(bool value) override;
  void ScrollRectToVisibleInParentFrame(
      const gfx::Rect& rect_to_scroll,
      blink::mojom::blink::ScrollIntoViewParamsPtr params) override;
  void BubbleLogicalScrollInParentFrame(
      blink::mojom::blink::ScrollDirection direction,
      ui::ScrollGranularity granularity) override;
  void DidAccessInitialDocument() override;
  void DidBlockNavigation(const KURL& blocked_url,
                          const KURL& initiator_url,
                          mojom::NavigationBlockedReason reason) override;
  void DidChangeLoadProgress(double load_progress) override;
  void DidFinishLoad(const KURL& validated_url) override;
  void DispatchLoad() override;
  void GoToEntryAtOffset(int32_t offset, bool has_user_gesture) override;
  void RenderFallbackContentInParentProcess() override;
  void UpdateTitle(const WTF::String& title,
                   base::i18n::TextDirection title_direction) override;
  void UpdateUserActivationState(
      mojom::blink::UserActivationUpdateType update_type,
      mojom::UserActivationNotificationType notification_type) override;
  void HandleAccessibilityFindInPageResult(
      mojom::blink::FindInPageResultAXParamsPtr params) override;
  void HandleAccessibilityFindInPageTermination() override;
  void DocumentOnLoadCompleted() override;
  void ForwardResourceTimingToParent(
      mojom::blink::ResourceTimingInfoPtr timing) override;
  void DidFinishDocumentLoad() override;
  void RunModalAlertDialog(const WTF::String& alert_message,
                           RunModalAlertDialogCallback callback) override;
  void RunModalConfirmDialog(const WTF::String& alert_message,
                             RunModalConfirmDialogCallback callback) override;
  void RunModalPromptDialog(const WTF::String& alert_message,
                            const WTF::String& default_value,
                            RunModalPromptDialogCallback callback) override;
  void RunBeforeUnloadConfirm(bool is_reload,
                              RunBeforeUnloadConfirmCallback callback) override;
  void UpdateFaviconURL(
      WTF::Vector<blink::mojom::blink::FaviconURLPtr> favicon_urls) override;
  void DownloadURL(mojom::blink::DownloadURLParamsPtr params) override;
  void FocusedElementChanged(bool is_editable_element,
                             const gfx::Rect& bounds_in_frame_widget,
                             blink::mojom::FocusType focus_type) override;
  void TextSelectionChanged(const WTF::String& text,
                            uint32_t offset,
                            const gfx::Range& range) override;
  void ShowPopupMenu(
      mojo::PendingRemote<mojom::blink::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      Vector<mojom::blink::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
  void DidLoadResourceFromMemoryCache(
      const KURL& url,
      const WTF::String& http_method,
      const WTF::String& mime_type,
      network::mojom::blink::RequestDestination request_destination) override;
  void DidChangeFrameOwnerProperties(
      const base::UnguessableToken& child_frame_token,
      mojom::blink::FrameOwnerPropertiesPtr frame_owner_properties) override;
  void DidChangeOpener(
      const base::Optional<base::UnguessableToken>& opener_frame) override;
  void DidChangeCSPAttribute(const base::UnguessableToken& child_frame_token,
                             network::mojom::blink::ContentSecurityPolicyPtr
                                 parsed_csp_attribute) override;
  void DidChangeFramePolicy(const base::UnguessableToken& child_frame_token,
                            const FramePolicy& frame_policy) override;
  void CapturePaintPreviewOfSubframe(
      const gfx::Rect& clip_rect,
      const base::UnguessableToken& guid) override;
  void Detach() override;

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::AssociatedReceiver<mojom::blink::LocalFrameHost> receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FAKE_LOCAL_FRAME_HOST_H_
