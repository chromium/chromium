// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"

#include "skia/public/mojom/skcolor.mojom-blink.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void FakeLocalFrameHost::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      mojom::blink::LocalFrameHost::Name_,
      WTF::BindRepeating(&FakeLocalFrameHost::BindFrameHostReceiver,
                         base::Unretained(this)));
}

void FakeLocalFrameHost::EnterFullscreen(
    mojom::blink::FullscreenOptionsPtr options,
    EnterFullscreenCallback callback) {
  std::move(callback).Run(true);
}

void FakeLocalFrameHost::ExitFullscreen() {}

void FakeLocalFrameHost::FullscreenStateChanged(
    bool is_fullscreen,
    mojom::blink::FullscreenOptionsPtr options) {}

void FakeLocalFrameHost::RegisterProtocolHandler(const WTF::String& scheme,
                                                 const ::blink::KURL& url,
                                                 bool user_gesture) {}

void FakeLocalFrameHost::UnregisterProtocolHandler(const WTF::String& scheme,
                                                   const ::blink::KURL& url,
                                                   bool user_gesture) {}

void FakeLocalFrameHost::DidDisplayInsecureContent() {}

void FakeLocalFrameHost::DidContainInsecureFormAction() {}

void FakeLocalFrameHost::MainDocumentElementAvailable(
    bool uses_temporary_zoom_level) {}

void FakeLocalFrameHost::SetNeedsOcclusionTracking(bool needs_tracking) {}
void FakeLocalFrameHost::SetVirtualKeyboardMode(
    ui::mojom::blink::VirtualKeyboardMode mode) {}

void FakeLocalFrameHost::VisibilityChanged(
    mojom::blink::FrameVisibility visibility) {}

void FakeLocalFrameHost::DidChangeThemeColor(
    std::optional<::SkColor> theme_color) {}

void FakeLocalFrameHost::DidChangeBackgroundColor(
    const SkColor4f& background_color,
    bool color_adjust) {}

void FakeLocalFrameHost::DidFailLoadWithError(const ::blink::KURL& url,
                                              int32_t error_code) {}

void FakeLocalFrameHost::DidFocusFrame() {}

void FakeLocalFrameHost::DidCallFocus() {}

void FakeLocalFrameHost::EnforceInsecureRequestPolicy(
    mojom::InsecureRequestPolicy policy_bitmap) {}

void FakeLocalFrameHost::EnforceInsecureNavigationsSet(
    const WTF::Vector<uint32_t>& set) {}

void FakeLocalFrameHost::SuddenTerminationDisablerChanged(
    bool present,
    blink::mojom::SuddenTerminationDisablerType disabler_type) {}

void FakeLocalFrameHost::HadStickyUserActivationBeforeNavigationChanged(
    bool value) {}

void FakeLocalFrameHost::ScrollRectToVisibleInParentFrame(
    const gfx::RectF& rect_to_scroll,
    blink::mojom::blink::ScrollIntoViewParamsPtr params) {}

void FakeLocalFrameHost::BubbleLogicalScrollInParentFrame(
    blink::mojom::blink::ScrollDirection direction,
    ui::ScrollGranularity granularity) {}

void FakeLocalFrameHost::DidBlockNavigation(
    const KURL& blocked_url,
    const KURL& initiator_url,
    mojom::NavigationBlockedReason reason) {}

void FakeLocalFrameHost::DidChangeLoadProgress(double load_progress) {}

void FakeLocalFrameHost::DidFinishLoad(const KURL& validated_url) {}

void FakeLocalFrameHost::DispatchLoad() {}

void FakeLocalFrameHost::GoToEntryAtOffset(
    int32_t offset,
    bool has_user_gesture,
    std::optional<blink::scheduler::TaskAttributionId>) {}

void FakeLocalFrameHost::UpdateTitle(
    const WTF::String& title,
    base::i18n::TextDirection title_direction) {}

void FakeLocalFrameHost::UpdateAppTitle(const WTF::String& app_title) {}

void FakeLocalFrameHost::UpdateUserActivationState(
    mojom::blink::UserActivationUpdateType update_type,
    mojom::UserActivationNotificationType notification_type) {}

void FakeLocalFrameHost::HandleAccessibilityFindInPageResult(
    mojom::blink::FindInPageResultAXParamsPtr params) {}

void FakeLocalFrameHost::HandleAccessibilityFindInPageTermination() {}

void FakeLocalFrameHost::DocumentOnLoadCompleted() {}

void FakeLocalFrameHost::ForwardResourceTimingToParent(
    mojom::blink::ResourceTimingInfoPtr timing) {}

void FakeLocalFrameHost::DidDispatchDOMContentLoadedEvent() {}

void FakeLocalFrameHost::RunModalAlertDialog(
    const WTF::String& alert_message,
    bool disable_third_party_subframe_suppresion,
    RunModalAlertDialogCallback callback) {
  std::move(callback).Run();
}

void FakeLocalFrameHost::RunModalConfirmDialog(
    const WTF::String& alert_message,
    bool disable_third_party_subframe_suppresion,
    RunModalConfirmDialogCallback callback) {
  std::move(callback).Run(true);
}

void FakeLocalFrameHost::RunModalPromptDialog(
    const WTF::String& alert_message,
    const WTF::String& default_value,
    bool disable_third_party_subframe_suppresion,
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
    bool is_richly_editable_element,
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

void FakeLocalFrameHost::CreateNewPopupWidget(
    mojo::PendingAssociatedReceiver<mojom::blink::PopupWidgetHost>
        popup_widget_host,
    mojo::PendingAssociatedReceiver<mojom::blink::WidgetHost> widget_host,
    mojo::PendingAssociatedRemote<mojom::blink::Widget> widget) {}

void FakeLocalFrameHost::ShowContextMenu(
    mojo::PendingAssociatedRemote<mojom::blink::ContextMenuClient>
        context_menu_client,
    const blink::UntrustworthyContextMenuParams& params) {}

void FakeLocalFrameHost::DidLoadResourceFromMemoryCache(
    const KURL& url,
    const WTF::String& http_method,
    const WTF::String& mime_type,
    network::mojom::blink::RequestDestination request_destination,
    bool include_credentials) {}

void FakeLocalFrameHost::DidChangeFrameOwnerProperties(
    const blink::FrameToken& child_frame_token,
    mojom::blink::FrameOwnerPropertiesPtr frame_owner_properties) {}

void FakeLocalFrameHost::DidChangeOpener(
    const std::optional<LocalFrameToken>& opener_frame) {}

void FakeLocalFrameHost::DidChangeIframeAttributes(
    const blink::FrameToken& child_frame_token,
    mojom::blink::IframeAttributesPtr) {}

void FakeLocalFrameHost::DidChangeFramePolicy(
    const blink::FrameToken& child_frame_token,
    const FramePolicy& frame_policy) {}

void FakeLocalFrameHost::CapturePaintPreviewOfSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {}

void FakeLocalFrameHost::SetCloseListener(
    mojo::PendingRemote<mojom::blink::CloseListener>) {}

void FakeLocalFrameHost::Detach() {}

void FakeLocalFrameHost::GetKeepAliveHandleFactory(
    mojo::PendingReceiver<mojom::blink::KeepAliveHandleFactory> receiver) {}

void FakeLocalFrameHost::DidAddMessageToConsole(
    mojom::ConsoleMessageLevel log_level,
    const WTF::String& message,
    uint32_t line_no,
    const WTF::String& source_id,
    const WTF::String& untrusted_stack_trace) {}

void FakeLocalFrameHost::FrameSizeChanged(const gfx::Size& frame_size) {}

void FakeLocalFrameHost::DidInferColorScheme(
    blink::mojom::PreferredColorScheme preferred_color_scheme) {}

void FakeLocalFrameHost::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::blink::LocalFrameHost>(
      std::move(handle)));
}

void FakeLocalFrameHost::DidChangeSrcDoc(
    const blink::FrameToken& child_frame_token,
    const WTF::String& srcdoc_value) {}

void FakeLocalFrameHost::ReceivedDelegatedCapability(
    blink::mojom::DelegatedCapability delegated_capability) {}

void FakeLocalFrameHost::SendFencedFrameReportingBeacon(
    const WTF::String& event_data,
    const WTF::String& event_type,
    const WTF::Vector<blink::FencedFrame::ReportingDestination>& destinations,
    bool cross_origin_exposed) {}

void FakeLocalFrameHost::SendFencedFrameReportingBeaconToCustomURL(
    const blink::KURL& destination_url,
    bool cross_origin_exposed) {}

void FakeLocalFrameHost::SetFencedFrameAutomaticBeaconReportEventData(
    blink::mojom::AutomaticBeaconType event_type,
    const WTF::String& event_data,
    const WTF::Vector<blink::FencedFrame::ReportingDestination>& destinations,
    bool once,
    bool cross_origin_exposed) {}

void FakeLocalFrameHost::DisableUntrustedNetworkInFencedFrame(
    DisableUntrustedNetworkInFencedFrameCallback callback) {
  std::move(callback).Run();
}

void FakeLocalFrameHost::ExemptUrlFromNetworkRevocationForTesting(
    const blink::KURL& exempted_url,
    ExemptUrlFromNetworkRevocationForTestingCallback callback) {
  std::move(callback).Run();
}

void FakeLocalFrameHost::SendLegacyTechEvent(
    const WTF::String& type,
    mojom::blink::LegacyTechEventCodeLocationPtr code_location) {}

void FakeLocalFrameHost::SendPrivateAggregationRequestsForFencedFrameEvent(
    const WTF::String& event_type) {}

void FakeLocalFrameHost::CreateFencedFrame(
    mojo::PendingAssociatedReceiver<mojom::blink::FencedFrameOwnerHost>,
    mojom::blink::RemoteFrameInterfacesFromRendererPtr remote_frame_interfaces,
    const RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  NOTREACHED_IN_MIGRATION() << "At the moment, FencedFrame is not used in any "
                               "unit tests, so this path should not be hit";
}

void FakeLocalFrameHost::ForwardFencedFrameEventAndUserActivationToEmbedder(
    const WTF::String& event_type) {
  NOTREACHED_IN_MIGRATION()
      << "ForwardFencedFrameEventToEmbedder is tested above the unit "
         "test layer";
}

void FakeLocalFrameHost::StartDragging(
    const blink::WebDragData& drag_data,
    blink::DragOperationsMask operations_allowed,
    const SkBitmap& bitmap,
    const gfx::Vector2d& cursor_offset_in_dip,
    const gfx::Rect& drag_obj_rect_in_dip,
    mojom::blink::DragEventSourceInfoPtr event_info) {}

void FakeLocalFrameHost::IssueKeepAliveHandle(
    mojo::PendingReceiver<mojom::blink::NavigationStateKeepAliveHandle>
        receiver) {}

void FakeLocalFrameHost::NotifyStorageAccessed(
    blink::mojom::StorageTypeAccessed storageType,
    bool blocked) {}

void FakeLocalFrameHost::RecordWindowProxyUsageMetrics(
    const blink::FrameToken& target_frame_token,
    blink::mojom::WindowProxyAccessType access_type) {}

}  // namespace blink
