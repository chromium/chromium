// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_LOCAL_FRAME_H_
#define CONTENT_PUBLIC_TEST_FAKE_LOCAL_FRAME_H_

#include <optional>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "net/http/http_connection_info.h"
#include "services/network/public/mojom/load_timing_info.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_api_history_entry_arrays.mojom.h"

namespace gfx {
class Point;
class Rect;
}

namespace content {

// This class implements a LocalFrame that can be attached to the
// AssociatedInterfaceProvider so that it will be called when the browser
// normally sends a request to the renderer process. But for a unittest
// setup it can be intercepted by this class.
class FakeLocalFrame : public blink::mojom::LocalFrame {
 public:
  FakeLocalFrame();
  ~FakeLocalFrame() override;

  void Init(blink::AssociatedInterfaceProvider* provider);

  // Flushes mojo messages on `receiver_`.
  void FlushMessages();

  // blink::mojom::LocalFrame:
  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) override;
  void SendInterventionReport(const std::string& id,
                              const std::string& message) override;
  void SetFrameOwnerProperties(
      blink::mojom::FrameOwnerPropertiesPtr properties) override;
  void NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type) override;
  void NotifyVirtualKeyboardOverlayRect(const gfx::Rect&) override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message,
                           bool discard_duplicates) override;
  void SwapInImmediately() override;
  void CheckCompleted() override;
  void StopLoading() override;
  void Collapse(bool collapsed) override;
  void EnableViewSourceMode() override;
  void Focus() override;
  void ClearFocusedElement() override;
  void CopyImageAt(const gfx::Point& window_point) override;
  void SaveImageAt(const gfx::Point& window_point) override;
  void ReportBlinkFeatureUsage(
      const std::vector<blink::mojom::WebFeature>&) override;
  void RenderFallbackContent() override;
  void BeforeUnload(bool is_reload, BeforeUnloadCallback callback) override;
  void MediaPlayerActionAt(const gfx::Point& location,
                           blink::mojom::MediaPlayerActionPtr action) override;
  void RequestVideoFrameAtWithBoundsHint(
      const gfx::Point& window_point,
      const gfx::Size& max_size,
      int max_area,
      RequestVideoFrameAtWithBoundsHintCallback callback) override;
  void PluginActionAt(const gfx::Point& location,
                      blink::mojom::PluginActionType action) override;
  void AdvanceFocusInFrame(blink::mojom::FocusType focus_type,
                           const std::optional<blink::RemoteFrameToken>&
                               source_frame_token) override;
  void AdvanceFocusForIME(blink::mojom::FocusType focus_type) override;
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation) override;
  void DidUpdateFramePolicy(const blink::FramePolicy& frame_policy) override;
  void OnFrameVisibilityChanged(
      blink::mojom::FrameVisibility visibility) override;
  void PostMessageEvent(
      const std::optional<blink::RemoteFrameToken>& source_frame_token,
      const std::u16string& source_origin,
      const std::u16string& target_origin,
      blink::TransferableMessage message) override;
  void JavaScriptMethodExecuteRequest(
      const std::u16string& object_name,
      const std::u16string& method_name,
      base::Value::List arguments,
      bool wants_result,
      JavaScriptMethodExecuteRequestCallback callback) override;
  void JavaScriptExecuteRequest(
      const std::u16string& javascript,
      bool wants_result,
      JavaScriptExecuteRequestCallback callback) override;
  void JavaScriptExecuteRequestForTests(
      const std::u16string& javascript,
      bool has_user_gesture,
      bool resolve_promises,
      bool honor_js_content_settings,
      int32_t world_id,
      JavaScriptExecuteRequestForTestsCallback callback) override;
  void JavaScriptExecuteRequestInIsolatedWorld(
      const std::u16string& javascript,
      bool wants_result,
      int32_t world_id,
      JavaScriptExecuteRequestInIsolatedWorldCallback callback) override;
  void GetSavableResourceLinks(
      GetSavableResourceLinksCallback callback) override;
#if BUILDFLAG(IS_MAC)
  void GetCharacterIndexAtPoint(const gfx::Point& point) override;
  void GetFirstRectForRange(const gfx::Range& range) override;
  void GetStringForRange(const gfx::Range& range,
                         GetStringForRangeCallback callback) override;
#endif
  void BindReportingObserver(
      mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver) override;
  void UpdateOpener(
      const std::optional<blink::FrameToken>& opener_frame_token) override;
  void MixedContentFound(
      const GURL& main_resource_url,
      const GURL& mixed_content_url,
      blink::mojom::RequestContextType request_context,
      bool was_allowed,
      const GURL& url_before_redirects,
      bool had_redirect,
      network::mojom::SourceLocationPtr source_location) override;
  void BindDevToolsAgent(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver)
      override;
#if BUILDFLAG(IS_ANDROID)
  void ExtractSmartClipData(const gfx::Rect& rect,
                            ExtractSmartClipDataCallback callback) override;
#endif
  void HandleRendererDebugURL(const GURL& url) override;
  void GetCanonicalUrlForSharing(
      base::OnceCallback<void(const std::optional<GURL>&)> callback) override;
  void GetOpenGraphMetadata(
      base::OnceCallback<void(blink::mojom::OpenGraphMetadataPtr)>) override;
  void SetNavigationApiHistoryEntriesForRestore(
      blink::mojom::NavigationApiHistoryEntryArraysPtr entry_arrays,
      blink::mojom::NavigationApiEntryRestoreReason restore_reason) override;
  void NotifyNavigationApiOfDisposedEntries(
      const std::vector<std::string>& keys) override;
  void TraverseCancelled(const std::string& navigation_api_key,
                         blink::mojom::TraverseCancelledReason reason) override;
  void DispatchNavigateEventForCrossDocumentTraversal(
      const GURL&,
      const std::string& page_state,
      bool is_browser_initiated) override;
  void SnapshotDocumentForViewTransition(
      const blink::ViewTransitionToken& transition_token,
      blink::mojom::PageSwapEventParamsPtr,
      SnapshotDocumentForViewTransitionCallback callback) override;
  void NotifyViewTransitionAbortedToOldDocument() override;
  void DispatchPageSwap(blink::mojom::PageSwapEventParamsPtr) override;
  void AddResourceTimingEntryForFailedSubframeNavigation(
      const ::blink::FrameToken& subframe_token,
      const GURL& initial_url,
      base::TimeTicks start_time,
      base::TimeTicks redirect_time,
      base::TimeTicks request_start,
      base::TimeTicks response_start,
      uint32_t response_code,
      const std::string& mime_type,
      const net::LoadTimingInfo& load_timing_info,
      net::HttpConnectionInfo connection_info,
      const std::string& alpn_negotiated_protocol,
      bool is_secure_transport,
      bool is_validated,
      const std::string& normalized_server_timing,
      const ::network::URLLoaderCompletionStatus& completion_status) override;
  void UpdatePrerenderURL(const ::GURL& matched_url,
                          UpdatePrerenderURLCallback callback) override;

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::AssociatedReceiver<blink::mojom::LocalFrame> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_LOCAL_FRAME_H_
