// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_local_frame.h"

#include "build/build_config.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/mojom/attributed_string.mojom.h"
#endif

namespace content {

FakeLocalFrame::FakeLocalFrame() {}

FakeLocalFrame::~FakeLocalFrame() {}

void FakeLocalFrame::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      blink::mojom::LocalFrame::Name_,
      base::BindRepeating(&FakeLocalFrame::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeLocalFrame::FlushMessages() {
  receiver_.FlushForTesting();
}

void FakeLocalFrame::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  std::move(callback).Run(std::u16string(), 0, 0);
}

void FakeLocalFrame::SendInterventionReport(const std::string& id,
                                            const std::string& message) {}

void FakeLocalFrame::SetFrameOwnerProperties(
    blink::mojom::FrameOwnerPropertiesPtr properties) {}

void FakeLocalFrame::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {}

void FakeLocalFrame::NotifyVirtualKeyboardOverlayRect(const gfx::Rect&) {}

void FakeLocalFrame::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool discard_duplicates) {}

void FakeLocalFrame::SwapInImmediately() {}

void FakeLocalFrame::CheckCompleted() {}

void FakeLocalFrame::StopLoading() {}

void FakeLocalFrame::Collapse(bool collapsed) {}

void FakeLocalFrame::EnableViewSourceMode() {}

void FakeLocalFrame::Focus() {}

void FakeLocalFrame::ClearFocusedElement() {}

void FakeLocalFrame::CopyImageAt(const gfx::Point& window_point) {}

void FakeLocalFrame::SaveImageAt(const gfx::Point& window_point) {}

void FakeLocalFrame::ReportBlinkFeatureUsage(
    const std::vector<blink::mojom::WebFeature>&) {}

void FakeLocalFrame::RenderFallbackContent() {}
void FakeLocalFrame::BeforeUnload(bool is_reload,
                                  BeforeUnloadCallback callback) {
  base::TimeTicks now = base::TimeTicks::Now();
  std::move(callback).Run(true /*leave the page*/, now, now);
}

void FakeLocalFrame::MediaPlayerActionAt(
    const gfx::Point& location,
    blink::mojom::MediaPlayerActionPtr action) {}

void FakeLocalFrame::RequestVideoFrameAtWithBoundsHint(
    const gfx::Point& window_point,
    const gfx::Size& max_size,
    int max_area,
    RequestVideoFrameAtWithBoundsHintCallback callback) {}

void FakeLocalFrame::PluginActionAt(const gfx::Point& location,
                                    blink::mojom::PluginActionType action) {}

void FakeLocalFrame::AdvanceFocusInFrame(
    blink::mojom::FocusType focus_type,
    const std::optional<blink::RemoteFrameToken>& source_frame_token) {}

void FakeLocalFrame::AdvanceFocusForIME(blink::mojom::FocusType focus_type) {}

void FakeLocalFrame::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation) {}

void FakeLocalFrame::DidUpdateFramePolicy(
    const blink::FramePolicy& frame_policy) {}

void FakeLocalFrame::OnFrameVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {}

void FakeLocalFrame::PostMessageEvent(
    const std::optional<blink::RemoteFrameToken>& source_frame_token,
    const std::u16string& source_origin,
    const std::u16string& target_origin,
    blink::TransferableMessage message) {}

void FakeLocalFrame::JavaScriptMethodExecuteRequest(
    const std::u16string& object_name,
    const std::u16string& method_name,
    base::Value::List arguments,
    bool wants_result,
    JavaScriptMethodExecuteRequestCallback callback) {}

void FakeLocalFrame::JavaScriptExecuteRequest(
    const std::u16string& javascript,
    bool wants_result,
    JavaScriptExecuteRequestCallback callback) {}

void FakeLocalFrame::JavaScriptExecuteRequestForTests(
    const std::u16string& javascript,
    bool has_user_gesture,
    bool resolve_promises,
    bool honor_js_content_settings,
    int32_t world_id,
    JavaScriptExecuteRequestForTestsCallback callback) {}

void FakeLocalFrame::JavaScriptExecuteRequestInIsolatedWorld(
    const std::u16string& javascript,
    bool wants_result,
    int32_t world_id,
    JavaScriptExecuteRequestInIsolatedWorldCallback callback) {}

void FakeLocalFrame::GetSavableResourceLinks(
    GetSavableResourceLinksCallback callback) {}

#if BUILDFLAG(IS_MAC)
void FakeLocalFrame::GetCharacterIndexAtPoint(const gfx::Point& point) {}
void FakeLocalFrame::GetFirstRectForRange(const gfx::Range& range) {}
void FakeLocalFrame::GetStringForRange(const gfx::Range& range,
                                       GetStringForRangeCallback callback) {
  std::move(callback).Run(nullptr, gfx::Point());
}
#endif

void FakeLocalFrame::BindReportingObserver(
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver) {}

void FakeLocalFrame::UpdateOpener(
    const std::optional<blink::FrameToken>& opener_frame_token) {}

void FakeLocalFrame::MixedContentFound(
    const GURL& main_resource_url,
    const GURL& mixed_content_url,
    blink::mojom::RequestContextType request_context,
    bool was_allowed,
    const GURL& url_before_redirects,
    bool had_redirect,
    network::mojom::SourceLocationPtr source_location) {}

void FakeLocalFrame::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver) {}

#if BUILDFLAG(IS_ANDROID)
void FakeLocalFrame::ExtractSmartClipData(
    const gfx::Rect& rect,
    ExtractSmartClipDataCallback callback) {
  std::move(callback).Run(std::u16string(), std::u16string(), gfx::Rect());
}
#endif

void FakeLocalFrame::HandleRendererDebugURL(const GURL& url) {}

void FakeLocalFrame::GetCanonicalUrlForSharing(
    base::OnceCallback<void(const std::optional<GURL>&)> callback) {}

void FakeLocalFrame::GetOpenGraphMetadata(
    base::OnceCallback<void(blink::mojom::OpenGraphMetadataPtr)>) {}

void FakeLocalFrame::SetNavigationApiHistoryEntriesForRestore(
    blink::mojom::NavigationApiHistoryEntryArraysPtr entry_arrays,
    blink::mojom::NavigationApiEntryRestoreReason restore_reason) {}

void FakeLocalFrame::NotifyNavigationApiOfDisposedEntries(
    const std::vector<std::string>& keys) {}

void FakeLocalFrame::TraverseCancelled(
    const std::string& navigation_api_key,
    blink::mojom::TraverseCancelledReason reason) {}

void FakeLocalFrame::DispatchNavigateEventForCrossDocumentTraversal(
    const GURL&,
    const std::string& page_state,
    bool is_browser_initiated) {}

void FakeLocalFrame::SnapshotDocumentForViewTransition(
    const blink::ViewTransitionToken& transition_token,
    blink::mojom::PageSwapEventParamsPtr,
    SnapshotDocumentForViewTransitionCallback callback) {}

void FakeLocalFrame::NotifyViewTransitionAbortedToOldDocument() {}

void FakeLocalFrame::DispatchPageSwap(blink::mojom::PageSwapEventParamsPtr) {}

void FakeLocalFrame::AddResourceTimingEntryForFailedSubframeNavigation(
    const ::blink::FrameToken& subframe_token,
    const GURL& initial_url,
    ::base::TimeTicks start_time,
    ::base::TimeTicks redirect_time,
    ::base::TimeTicks request_start,
    ::base::TimeTicks response_start,
    uint32_t response_code,
    const std::string& mime_type,
    const ::net::LoadTimingInfo& load_timing_info,
    ::net::HttpConnectionInfo connection_info,
    const std::string& alpn_negotiated_protocol,
    bool is_secure_transport,
    bool is_validated,
    const std::string& normalized_server_timing,
    const ::network::URLLoaderCompletionStatus& completion_status) {}

void FakeLocalFrame::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<blink::mojom::LocalFrame>(
      std::move(handle)));
}

void FakeLocalFrame::UpdatePrerenderURL(const ::GURL& matched_url,
                                        UpdatePrerenderURLCallback callback) {
  std::move(callback).Run();
}

}  // namespace content
