// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/containers/id_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/can_commit_status.h"
#include "content/browser/feature_observer.h"
#include "content/browser/idle/idle_manager_impl.h"
#include "content/browser/net/cross_origin_opener_policy_reporter.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/dom_automation_controller.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_delete_intention.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input/input_injector.mojom-forward.h"
#include "content/common/navigation_params.mojom.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/peak_gpu_memory_tracker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/javascript_dialog_type.h"
#include "media/mojo/mojom/interface_factory.mojom-forward.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-forward.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/http/http_response_headers.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/feature_policy/document_policy.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"
#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-forward.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "third_party/blink/public/mojom/screen_enumeration/screen_enumeration.mojom-forward.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-forward.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "services/device/public/mojom/nfc.mojom.h"
#else
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#endif

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
#include "media/mojo/mojom/remoting.mojom-forward.h"
#endif

class GURL;

namespace blink {
class AssociatedInterfaceProvider;
class AssociatedInterfaceRegistry;
struct FramePolicy;
struct TransferableMessage;

namespace mojom {
class CacheStorage;
class GeolocationService;
class WebUsbService;
}  // namespace mojom
}  // namespace blink

namespace gfx {
class Range;
}

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace ui {
class ClipboardFormatType;
}

namespace content {
class AgentSchedulingGroupHost;
class AppCacheNavigationHandle;
class AuthenticatorImpl;
class BackForwardCacheMetrics;
class CrossOriginEmbedderPolicyReporter;
class FrameTree;
class FrameTreeNode;
class GeolocationServiceImpl;
class KeepAliveHandleFactory;
class MediaInterfaceProxy;
class NavigationEntryImpl;
class NavigationRequest;
class PermissionServiceContext;
class Portal;
class PrefetchedSignedExchangeCache;
class PresentationServiceImpl;
class PushMessagingManager;
class RenderFrameHostDelegate;
class RenderFrameHostImpl;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class ScreenEnumerationImpl;
class SensorProviderProxyImpl;
class SerialService;
class SpeechSynthesisImpl;
class TimeoutMonitor;
class WebAuthRequestSecurityChecker;
class WebBluetoothServiceImpl;
class WebBundleHandle;
class WebBundleHandleTracker;
struct UntrustworthyContextMenuParams;
struct PendingNavigation;
struct RenderFrameHostOrProxy;
struct ResourceTimingInfo;
struct SubresourceLoaderParams;

// True if feature-flags indicate that we should replace crashed RFHs with new
// instances rather than reusing them. See http://crbug.com/981339.
CONTENT_EXPORT bool ShouldCreateNewHostForCrashedFrame();

// To be called when a RenderFrameHostImpl receives an event.
// Provides the host, the event fired, and which node id the event was for.
typedef base::RepeatingCallback<
    void(RenderFrameHostImpl*, ax::mojom::Event, int)>
    AccessibilityCallbackForTesting;

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      public base::SupportsUserData,
      public mojom::FrameHost,
      public mojom::RenderAccessibilityHost,
      public mojom::DomAutomationControllerHost,
      public BrowserAccessibilityDelegate,
      public RenderProcessHostObserver,
      public SiteInstanceImpl::Observer,
      public service_manager::mojom::InterfaceProvider,
      public blink::mojom::BackForwardCacheControllerHost,
      public blink::mojom::LocalFrameHost,
      public network::CSPContext,
      public blink::mojom::LocalMainFrameHost,
      public ui::AXActionHandlerBase,
      public network::mojom::CookieAccessObserver {
 public:
  using AXTreeSnapshotCallback =
      base::OnceCallback<void(const ui::AXTreeUpdate&)>;
  using JavaScriptDialogCallback =
      content::JavaScriptDialogManager::DialogClosedCallback;

  // Callback used with IsClipboardPasteAllowed() method.
  using ClipboardPasteAllowed = ContentBrowserClient::ClipboardPasteAllowed;
  using IsClipboardPasteAllowedCallback =
      ContentBrowserClient::IsClipboardPasteAllowedCallback;

  // An accessibility reset is only allowed to prevent very rare corner cases
  // or race conditions where the browser and renderer get out of sync. If
  // this happens more than this many times, kill the renderer.
  // Can be set to 0 to fail immediately during tests.
  static int max_accessibility_resets_;

  static RenderFrameHostImpl* FromID(GlobalFrameRoutingId id);
  static RenderFrameHostImpl* FromID(int process_id, int routing_id);
  static RenderFrameHostImpl* FromFrameToken(
      int process_id,
      const base::UnguessableToken& frame_token);
  static RenderFrameHostImpl* FromAXTreeID(ui::AXTreeID ax_tree_id);
  static RenderFrameHostImpl* FromOverlayRoutingToken(
      const base::UnguessableToken& token);

  // Clears the all prefetched cached signed exchanges.
  static void ClearAllPrefetchedSignedExchangeCache();

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  int GetRoutingID() override;
  const base::UnguessableToken& GetFrameToken() override;
  ui::AXTreeID GetAXTreeID() override;
  SiteInstanceImpl* GetSiteInstance() override;
  RenderProcessHost* GetProcess() override;
  RenderWidgetHostView* GetView() override;
  RenderFrameHostImpl* GetParent() override;
  RenderFrameHostImpl* GetMainFrame() override;
  std::vector<RenderFrameHost*> GetFramesInSubtree() override;
  bool IsDescendantOf(RenderFrameHost*) override;
  int GetFrameTreeNodeId() override;
  base::UnguessableToken GetDevToolsFrameToken() override;
  base::Optional<base::UnguessableToken> GetEmbeddingToken() override;
  const std::string& GetFrameName() override;
  bool IsFrameDisplayNone() override;
  const base::Optional<gfx::Size>& GetFrameSize() override;
  size_t GetFrameDepth() override;
  bool IsCrossProcessSubframe() override;
  const GURL& GetLastCommittedURL() override;
  const url::Origin& GetLastCommittedOrigin() override;
  const net::NetworkIsolationKey& GetNetworkIsolationKey() override;
  const net::IsolationInfo& GetIsolationInfoForSubresources() override;
  gfx::NativeView GetNativeView() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  void ExecuteJavaScriptMethod(const base::string16& object_name,
                               const base::string16& method_name,
                               base::Value arguments,
                               JavaScriptResultCallback callback) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         JavaScriptResultCallback callback) override;
  void ExecuteJavaScriptInIsolatedWorld(const base::string16& javascript,
                                        JavaScriptResultCallback callback,
                                        int32_t world_id) override;
  void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      JavaScriptResultCallback callback,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) override;
  void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  void InsertVisualStateCallback(VisualStateCallback callback) override;
  void CopyImageAt(int x, int y) override;
  void SaveImageAt(int x, int y) override;
  RenderViewHost* GetRenderViewHost() override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  content::PageVisibilityState GetVisibilityState() override;
  bool IsRenderFrameCreated() override;
  bool IsRenderFrameLive() override;
  bool IsCurrent() override;
  bool IsInactiveAndDisallowReactivation() override;
  size_t GetProxyCount() override;
  bool HasSelection() override;
  void RequestTextSurroundingSelection(
      blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback,
      int max_length) override;
  void SendInterventionReport(const std::string& id,
                              const std::string& message) override;
  WebUI* GetWebUI() override;
  void AllowBindings(int binding_flags) override;
  int GetEnabledBindings() override;
  void SetWebUIProperty(const std::string& name,
                        const std::string& value) override;
  void DisableBeforeUnloadHangMonitorForTesting() override;
  bool IsBeforeUnloadHangMonitorDisabledForTesting() override;
  bool GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType disabler_type) override;
  bool IsFeatureEnabled(blink::mojom::FeaturePolicyFeature feature) override;
  bool IsFeatureEnabled(blink::mojom::DocumentPolicyFeature feature) override;
  bool IsFeatureEnabled(blink::mojom::DocumentPolicyFeature feature,
                        blink::PolicyValue threshold_value) override;
  void ViewSource() override;
  void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point&,
      const blink::mojom::MediaPlayerAction& action) override;
  bool CreateNetworkServiceDefaultFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver) override;
  void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      base::flat_set<url::Origin> isolated_world_origins,
      bool push_to_renderer_now) override;
  bool IsSandboxed(network::mojom::WebSandboxFlags flags) override;
  void FlushNetworkAndNavigationInterfacesForTesting() override;
  void PrepareForInnerWebContentsAttach(
      PrepareForInnerWebContentsAttachCallback callback) override;
  void UpdateSubresourceLoaderFactories() override;
  blink::mojom::FrameOwnerElementType GetFrameOwnerElementType() override;
  bool HasTransientUserActivation() override;
  void NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type) override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;
  bool Reload() override;
  bool IsDOMContentLoaded() override;
  void UpdateAdFrameType(blink::mojom::AdFrameType ad_frame_type) override;
  blink::mojom::AuthenticatorStatus PerformGetAssertionWebAuthSecurityChecks(
      const std::string& relying_party_id,
      const url::Origin& effective_origin) override;
  blink::mojom::AuthenticatorStatus PerformMakeCredentialWebAuthSecurityChecks(
      const std::string& relying_party_id,
      const url::Origin& effective_origin) override;
  void SetIsXrOverlaySetup() override;
  bool IsInBackForwardCache() override;
  ukm::SourceId GetPageUkmSourceId() override;
  StoragePartition* GetStoragePartition() override;
  BrowserContext* GetBrowserContext() override;
  void ReportHeavyAdIssue(blink::mojom::HeavyAdResolutionStatus resolution,
                          blink::mojom::HeavyAdReason reason) override;
  void AsValueInto(base::trace_event::TracedValue* traced_value) override;

  // Determines if a clipboard paste using |data| of type |data_type| is allowed
  // in this renderer frame.  The implementation delegates to
  // RenderFrameHostDelegate::IsClipboardPasteAllowed().  See the description of
  // the latter method for complete details.
  void IsClipboardPasteAllowed(const ui::ClipboardFormatType& data_type,
                               const std::string& data,
                               IsClipboardPasteAllowedCallback callback);

  void SendAccessibilityEventsToManager(
      const AXEventNotificationDetails& details);

  void EvictFromBackForwardCacheWithReason(
      BackForwardCacheMetrics::NotRestoredReason reason);
  void EvictFromBackForwardCacheWithReasons(
      const BackForwardCacheCanStoreDocumentResult& can_store);

  // Returns the current WebPreferences for the WebContents associated with this
  // RenderFrameHost. Will create one if it does not exist (and update all the
  // renderers with the newly computed value).
  blink::web_pref::WebPreferences GetOrCreateWebPreferences();

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // BrowserAccessibilityDelegate
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() override;
  float AccessibilityGetDeviceScaleFactor() override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  WebContents* AccessibilityWebContents() override;
  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      ax::mojom::Event opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                              int hit_node_id)> opt_callback) override;
  bool AccessibilityIsMainFrame() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // SiteInstanceImpl::Observer
  void RenderProcessGone(SiteInstanceImpl* site_instance,
                         const ChildProcessTerminationInfo& info) override;

  // network::CSPContext
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation_params) override;
  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override;
  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const override;

  // ui::AXActionHandlerBase:
  void PerformAction(const ui::AXActionData& data) override;
  bool RequiresPerformActionPointInPixels() const override;

  // Creates a RenderFrame in the renderer process.
  bool CreateRenderFrame(
      int previous_routing_id,
      const base::Optional<base::UnguessableToken>& opener_frame_token,
      int parent_routing_id,
      int previous_sibling_routing_id);

  // Deletes the RenderFrame in the renderer process.
  // Postcondition: |IsPendingDeletion()| is true.
  void DeleteRenderFrame(FrameDeleteIntention intent);

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.
  void SetRenderFrameCreated(bool created);

  // Called for renderer-created windows to resume requests from this frame,
  // after they are blocked in RenderWidgetHelper::CreateNewWindow.
  void Init();

  // This needs to be called to make sure that the parent-child relationship
  // between frames is properly established both for cross-process iframes as
  // well as for inner web contents (i.e. right after having attached it to the
  // outer web contents), so that navigating the whole tree is possible.
  //
  // It's safe to call this method multiple times, or even before the embedding
  // token has been set for this frame, to account for the realistic possibility
  // that inner web contents can became attached to the outer one both before
  // and after the embedding token has been set.
  void PropagateEmbeddingTokenToParentFrame();

  // Returns true if the frame recently plays an audio.
  bool is_audible() const { return is_audible_; }

  // Toggles the audible state of this render frame. This should only be called
  // from AudioStreamMonitor, and should not be invoked with the same value
  // successively.
  void OnAudibleStateChanged(bool is_audible);

  int routing_id() const { return routing_id_; }

  // Called when this frame has added a child. This is a continuation of an IPC
  // that was partially handled on the IO thread (to allocate |new_routing_id|,
  // |frame_token| and |devtools_frame_token|), and is forwarded here. The
  // renderer has already been told to create a RenderFrame with the specified
  // ID values. |interface_provider_receiver| is the receiver end of the
  // InterfaceProvider interface that the RenderFrameHost corresponding to the
  // child frame should bind to expose services to the renderer process. The
  // caller takes care of sending down the client end of the pipe to the child
  // RenderFrame to use. |browser_interface_broker_receiver| is the receiver end
  // of BrowserInterfaceBroker interface in the child frame. RenderFrameHost
  // should bind this receiver to expose services to the renderer process. The
  // caller takes care of sending down the client end of the pipe to the child
  // RenderFrame to use.
  void OnCreateChildFrame(
      int new_routing_id,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider_receiver,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const blink::FramePolicy& frame_policy,
      const blink::mojom::FrameOwnerProperties& frame_owner_properties,
      blink::mojom::FrameOwnerElementType owner_type);

  // Update this frame's state at the appropriate time when a navigation
  // commits. This is called by Navigator::DidNavigate as a helper, in the
  // midst of a DidCommitProvisionalLoad call. If |did_create_new_document| is
  // true the navigation was not same-document and was not served from the
  // back-forward cache.
  void DidNavigate(const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
                   NavigationRequest* navigation_request,
                   bool did_create_new_document);

  AgentSchedulingGroupHost& agent_scheduling_group() {
    return agent_scheduling_group_;
  }
  RenderViewHostImpl* render_view_host() { return render_view_host_.get(); }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTree* frame_tree() const { return frame_tree_; }
  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  // Methods to add/remove/reset/query child FrameTreeNodes of this frame.
  // See class-level comment for FrameTreeNode for how the frame tree is
  // represented.
  size_t child_count() { return children_.size(); }
  FrameTreeNode* child_at(size_t index) const { return children_[index].get(); }
  FrameTreeNode* AddChild(std::unique_ptr<FrameTreeNode> child,
                          int process_id,
                          int frame_routing_id,
                          const base::UnguessableToken& frame_token);
  void RemoveChild(FrameTreeNode* child);
  void ResetChildren();

  // Allows FrameTreeNode::SetCurrentURL to update this frame's last committed
  // URL.  Do not call this directly, since we rely on SetCurrentURL to track
  // whether a real load has committed or not.
  void SetLastCommittedUrl(const GURL& url);

  // The most recent non-net-error URL to commit in this frame.  In almost all
  // cases, use GetLastCommittedURL instead.
  const GURL& last_successful_url() { return last_successful_url_; }

  // Return the http method of the last committed navigation.
  const std::string& last_http_method() { return last_http_method_; }

  // Return the http status code of the last committed navigation.
  int last_http_status_code() { return last_http_status_code_; }

  // Returns true if |dest_url_info| should be considered the same site as the
  // current contents of this frame. This is the primary entry point for
  // determining if a navigation to |dest_url_info| should stay in this
  // RenderFrameHost's SiteInstance.
  //
  // |is_coop_coep_cross_origin_isolated| should be true if the response for
  // |dest_url| has set COOP and COEP headers to same-origin and require-corp
  // respectively.
  // if |is_coop_coep_cross_origin_isolated| is true,
  // |coop_coep_cross_origin_isolated_origin| indicates the top level origin
  // of the page.
  bool IsNavigationSameSite(
      const UrlInfo& dest_url_info,
      bool is_coop_coep_cross_origin_isolated,
      base::Optional<url::Origin> coop_coep_cross_origin_isolated_origin);

  // Returns |frame_origin| if this frame is the top (i.e. root) frame in the
  // frame tree. Otherwise, it returns the top frame's origin.
  const url::Origin& ComputeTopFrameOrigin(
      const url::Origin& frame_origin) const;

  // Computes the IsolationInfo this frame to |destination|.
  net::IsolationInfo ComputeIsolationInfoForNavigation(
      const GURL& destination) const;

  // Computes site_for_cookies for this frame. A non-empty result denotes which
  // domains are considered first-party to the top-level site when resources are
  // loaded inside this frame. An empty result means that nothing will be
  // first-party, as the frame hierarchy makes this context third-party already.
  //
  // The result can be used to check if cookies (including storage APIs and
  // shared/service workers) are accessible.
  net::SiteForCookies ComputeSiteForCookies();

  // Allows overriding the last committed origin in tests.
  void SetLastCommittedOriginForTesting(const url::Origin& origin);

  // Fetch the link-rel canonical URL to be used for sharing to external
  // applications.
  void GetCanonicalUrlForSharing(
      mojom::Frame::GetCanonicalUrlForSharingCallback callback);

  // Get HTML data for this RenderFrame by serializing contents on the renderer
  // side and replacing all links to both same-site and cross-site resources
  // with paths to local copies as specified by |url_map| and |frame_token_map|.
  void GetSerializedHtmlWithLocalLinks(
      const base::flat_map<GURL, base::FilePath>& url_map,
      const base::flat_map<base::UnguessableToken, base::FilePath>&
          frame_token_map,
      bool save_with_empty_url,
      mojo::PendingRemote<mojom::FrameHTMLSerializerHandler>
          serializer_handler);

  // Returns the associated WebUI or null if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }
  WebUI::TypeID web_ui_type() const { return web_ui_type_; }

  // Enable Mojo JavaScript bindings in the renderer process. It will be
  // effective on the first creation of script context after the call is made.
  // If called at frame creation time (RenderFrameCreated) or just before a
  // document is committed (ReadyToCommitNavigation), the resulting document
  // will have the JS bindings enabled.
  void EnableMojoJsBindings();

  // Returns true if this is a main RenderFrameHost. True if and only if this
  // RenderFrameHost doesn't have a parent.
  bool is_main_frame() const { return !parent_; }

  // Returns this RenderFrameHost's loading state. This method is only used by
  // FrameTreeNode. The proper way to check whether a frame is loading is to
  // call FrameTreeNode::IsLoading.
  bool is_loading() const { return is_loading_; }

  // Returns true if this is a top-level frame, or if this frame's RenderFrame
  // is in a different process from its parent frame. Local roots are
  // distinguished by owning a RenderWidgetHost, which manages input events
  // and painting for this frame and its contiguous local subtree in the
  // renderer process.
  bool is_local_root() const { return !!GetLocalRenderWidgetHost(); }

  // Returns the RenderWidgetHostImpl attached to this frame or the nearest
  // ancestor frame, which could potentially be the root. For most input
  // and rendering related purposes, GetView() should be preferred and
  // RenderWidgetHostViewBase methods used. GetRenderWidgetHost() will not
  // return a nullptr, whereas GetView() potentially will (for instance,
  // after a renderer crash).
  //
  // This method crashes if this RenderFrameHostImpl does not own a
  // a RenderWidgetHost and nor does any of its ancestors. That would
  // typically mean that the frame has been detached from the frame tree.
  virtual RenderWidgetHostImpl* GetRenderWidgetHost();

  GlobalFrameRoutingId GetGlobalFrameRoutingId();

  media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
  GetRecordAggregateWatchTimeCallback();

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.
  int nav_entry_id() const { return nav_entry_id_; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

  // Return true if this contains at least one NavigationRequest waiting to
  // commit in this RenderFrameHost.
  bool HasPendingCommitNavigation() const;

  // Return true if Unload() was called on the frame or one of its ancestors.
  // If true, this corresponds either to unload handlers running for this
  // RenderFrameHost (LifecycleState::kRunningUnloadHandlers) or when this
  // RenderFrameHost is ready to be deleted (LifecycleState::kReadyToBeDeleted).
  bool IsPendingDeletion();

  // A NavigationRequest for a pending cross-document navigation in this frame,
  // if any. This is cleared when the navigation commits.
  NavigationRequest* navigation_request() { return navigation_request_.get(); }

  // A NavigationRequest for a pending same-document navigation in this frame,
  // if any. This is cleared when the navigation commits.
  NavigationRequest* same_document_navigation_request() {
    return same_document_navigation_request_.get();
  }

  // Resets the NavigationRequests stored in this RenderFrameHost.
  void ResetNavigationRequests();

  // Called when a navigation is ready to commit in this
  // RenderFrameHost. Transfers ownership of the NavigationRequest associated
  // with the navigation to this RenderFrameHost.
  void SetNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Tells the renderer that this RenderFrame is being replaced with one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // RenderFrameProxy to replace the RenderFrame and set it to |is_loading|
  // state. The renderer process keeps the RenderFrameProxy object around as a
  // placeholder while the frame is rendered in a different process.
  //
  // There should always be a |proxy| to replace the old RenderFrameHost. If
  // there are no remaining active views in the process, the proxy will be
  // short-lived and will be deleted when the unload ACK is received.
  //
  // RenderDocument: After a local<->local swap, this function is called with a
  // null |proxy|. It executes common cleanup and marks this RenderFrameHost to
  // have completed its unload handler. The RenderFrameHost may be immediately
  // deleted or deferred depending on its children's unload status.
  void Unload(RenderFrameProxyHost* proxy, bool is_loading);

  // Remove this frame and its children. This happens asynchronously, an IPC
  // round trip with the renderer process is needed to ensure children's unload
  // handlers are run.
  // Postcondition: |IsPendingDeletion()| is true.
  void DetachFromProxy();

  // Whether an ongoing navigation in this frame is waiting for a BeforeUnload
  // completion callback either from this RenderFrame or from one of its
  // subframes.
  bool is_waiting_for_beforeunload_completion() const {
    return is_waiting_for_beforeunload_completion_;
  }

  // True if more than |beforeunload_timeout_delay_| has elapsed since starting
  // beforeunload. This may be true before |beforeunload_timeout_| actually
  // fires, as the task can be delayed by task scheduling. See crbug.com/1056257
  bool BeforeUnloadTimedOut() const;

  // Whether the RFH is waiting for an unload ACK from the renderer.
  bool IsWaitingForUnloadACK() const;

  // Called when either the Unload() request has been acknowledged or has timed
  // out.
  void OnUnloaded();

  // Stop the load in progress.
  void Stop();

  // Defines different states the RenderFrameHost can be in during its lifetime
  // i.e., from point of creation to deletion. See |SetLifecycleState|.
  enum class LifecycleState {
    // This state corresponds to when a speculative RenderFrameHost is created
    // for an ongoing navigation (to new URL) but hasn't been swapped in the
    // frame tree yet, mainly created for performance optimization. The frame
    // can only be created in this state and no transitions happen to this
    // state.
    //
    // Transitions from this state happen to either kActive (when navigation
    // commits) or kReadyToBeDeleted (when the navigation redirects
    // or gets cancelled). Note that the term speculative is used, because the
    // navigation might be canceled or redirected and the RenderFrameHost might
    // get deleted before being used.
    kSpeculative,

    // This state corresponds to when a RenderFrameHost is the current one in
    // its RenderFrameHostManager and FrameTreeNode. In this state,
    // RenderFrameHost is visible to the user. Transition to kActive state may
    // happen from either kSpeculative (when navigation commits) or
    // kInBackForwardCache (when restoring from BackForwardCache) states.
    //
    // RenderFrameHost can also be created in this state for an empty document
    // in a FrameTreeNode (e.g initializing root and child in an empty
    // FrameTree).
    kActive,

    // This state corresponds to when RenderFrameHost is stored in
    // BackForwardCache. This happens when the user navigates away from a
    // document, so that the RenderFrameHost can be re-used after a history
    // navigation. Transition to this state happens only from kActive state.
    kInBackForwardCache,

    // This state corresponds to when RenderFrameHost has started running unload
    // handlers (this includes handlers for the "unload", "pagehide", and
    // "visibilitychange" events). An event such as navigation commit or
    // detaching the frame causes the RenderFrameHost to transition to this
    // state. Then, the RenderFrameHost sends IPCs to the renderer process to
    // execute unload handlers and deletes the RenderFrame. The RenderFrameHost
    // waits for an ACK from the renderer process, either
    // FrameHostMsg_Unload_ACK for a navigating frame or FrameHostMsg_Detach for
    // its subframes, after which the RenderFrameHost transitions to
    // kReadyToBeDeleted state.
    //
    // Transition to this state happens only from kActive state. Note that
    // eviction from BackForwardCache does not run unload handlers, and
    // kInBackForwardCache moves to kReadyToBeDeleted.
    kRunningUnloadHandlers,

    // This state corresponds to when RenderFrameHost has completed running the
    // unload handlers. Once all the descendant frames in other processes are
    // gone, this RenderFrameHost will delete itself. Transition to this state
    // may happen from one of kSpeculative, kActive, kInBackForwardCache or
    // kRunningUnloadHandlers states.
    kReadyToBeDeleted,
  };
  LifecycleState lifecycle_state() const { return lifecycle_state_; }
  void SetLifecycleStateToActive();

  enum class BeforeUnloadType {
    BROWSER_INITIATED_NAVIGATION,
    RENDERER_INITIATED_NAVIGATION,
    TAB_CLOSE,
    // This reason is used before a tab is discarded in order to free up
    // resources. When this is used and the handler returns a non-empty string,
    // the confirmation dialog will not be displayed and the discard will
    // automatically be canceled.
    DISCARD,
    // This reason is used when preparing a FrameTreeNode for attaching an inner
    // delegate. In this case beforeunload is dispatched in the frame and all
    // the nested child frames.
    INNER_DELEGATE_ATTACH,
  };

  // Runs the beforeunload handler for this frame and its subframes. |type|
  // indicates whether this call is for a navigation or tab close. |is_reload|
  // indicates whether the navigation is a reload of the page.  If |type|
  // corresponds to tab close and not a navigation, |is_reload| should be
  // false.
  void DispatchBeforeUnload(BeforeUnloadType type, bool is_reload);

  // Simulate beforeunload completion callback on behalf of renderer if it's
  // unrenresponsive.
  void SimulateBeforeUnloadCompleted(bool proceed);

  // Returns true if a call to DispatchBeforeUnload will actually send the
  // BeforeUnload IPC.  This can be called on a main frame or subframe.  If
  // |check_subframes_only| is false, it covers handlers for the frame
  // itself and all its descendants.  If |check_subframes_only| is true, it
  // only checks the frame's descendants but not the frame itself.
  bool ShouldDispatchBeforeUnload(bool check_subframes_only);

  // Allow tests to override how long to wait for beforeunload completion
  // callbacks to be invoked before timing out.
  void SetBeforeUnloadTimeoutDelayForTesting(const base::TimeDelta& timeout);

  // Update the frame's opener in the renderer process in response to the
  // opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener();

  // Set this frame as focused in the renderer process.  This supports
  // cross-process window.focus() calls.
  void SetFocusedFrame();

  // Continues sequential focus navigation in this frame. |source_proxy|
  // represents the frame that requested a focus change. It must be in the same
  // process as this or |nullptr|.
  void AdvanceFocus(blink::mojom::FocusType type,
                    RenderFrameProxyHost* source_proxy);

  // Get the accessibility mode from the delegate and Send a message to the
  // renderer process to change the accessibility mode.
  void UpdateAccessibilityMode();

#if defined(OS_ANDROID)
  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
  using ExtractSmartClipDataCallback = base::OnceCallback<
      void(const base::string16&, const base::string16&, const gfx::Rect&)>;

  void RequestSmartClipExtract(ExtractSmartClipDataCallback callback,
                               gfx::Rect rect);

  void OnSmartClipDataExtracted(int32_t callback_id,
                                const base::string16& text,
                                const base::string16& html,
                                const gfx::Rect& clip_rect);
#endif  // defined(OS_ANDROID)

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode.
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                             ui::AXMode ax_mode);

  // Resets the accessibility serializer in the renderer.
  void AccessibilityReset();

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process.
  void SetAccessibilityCallbackForTesting(
      const AccessibilityCallbackForTesting& callback);

  // Called when the metadata about the accessibility tree for this frame
  // changes due to a browser-side change, as opposed to due to an IPC from
  // a renderer.
  void UpdateAXTreeData();

  // Set the AX tree ID of the embedder RFHI, if this is a browser plugin guest.
  void set_browser_plugin_embedder_ax_tree_id(ui::AXTreeID ax_tree_id) {
    browser_plugin_embedder_ax_tree_id_ = ax_tree_id;
  }

  // Access the BrowserAccessibilityManager if it already exists.
  BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // null.
  BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

  // Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  // |subresource_loader_params| is used in network service land to pass
  // the parameters to create a custom subresource loader in the renderer
  // process, e.g. by AppCache etc.
  void CommitNavigation(
      NavigationRequest* navigation_request,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_view_source,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      const base::UnguessableToken& devtools_navigation_token,
      std::unique_ptr<WebBundleHandle> web_bundle_handle);

  // Indicates that a navigation failed and that this RenderFrame should display
  // an error page.
  void FailedNavigation(NavigationRequest* navigation_request,
                        const mojom::CommonNavigationParams& common_params,
                        const mojom::CommitNavigationParams& commit_params,
                        bool has_stale_copy_in_cache,
                        int error_code,
                        const base::Optional<std::string>& error_page_content);

  // Seneds a renderer-debug URL to the renderer process for handling.
  void HandleRendererDebugURL(const GURL& url);

  // Sets up the Mojo connection between this instance and its associated render
  // frame if it has not yet been set up.
  void SetUpMojoIfNeeded();

  // Tears down the browser-side state relating to the Mojo connection between
  // this instance and its associated render frame.
  void InvalidateMojoConnection();

  // Returns whether the frame is focused. A frame is considered focused when it
  // is the parent chain of the focused frame within the frame tree. In
  // addition, its associated RenderWidgetHost has to be focused.
  bool IsFocused();

  // Creates a WebUI for this RenderFrameHost based on the provided |dest_url|
  // if required. Returns true if a new WebUI was created.
  // If this is a history navigation its NavigationEntry bindings should be
  // provided through |entry_bindings| to allow verifying that they are not
  // being set differently this time around. Otherwise |entry_bindings| should
  // be set to NavigationEntryImpl::kInvalidBindings so that no checks are done.
  bool CreateWebUI(const GURL& dest_url, int entry_bindings);

  // Destroys WebUI instance and resets related data.
  void ClearWebUI();

  // Returns the Mojo ImageDownloader service.
  const mojo::Remote<blink::mojom::ImageDownloader>& GetMojoImageDownloader();

  // Returns remote to renderer side FindInPage associated with this frame.
  const mojo::AssociatedRemote<blink::mojom::FindInPage>& GetFindInPage();

  // Returns associated remote for the blink::mojom::LocalFrame Mojo interface.
  const mojo::AssociatedRemote<blink::mojom::LocalFrame>&
  GetAssociatedLocalFrame();

  // Returns associated remote for the blink::mojom::LocalMainFrame Mojo
  // interface. May be overridden by subclasses, e.g. tests which wish to
  // intercept outgoing local main frame messages.
  virtual blink::mojom::LocalMainFrame* GetAssociatedLocalMainFrame();

  // Returns remote to blink::mojom::HighPriorityLocalFrame Mojo interface. Note
  // this interface is highly experimental and is being tested to address
  // crbug.com/1042118. It is not an associated interface and may be actively
  // reordered. GetAssociatedLocalFrame() should be used in most cases and any
  // additional use cases of this interface should probably consider discussing
  // with navigation-dev@chromium.org first.
  const mojo::Remote<blink::mojom::HighPriorityLocalFrame>&
  GetHighPriorityLocalFrame();

  // Returns associated remote for the blink::mojom::FrameBindingsControl Mojo
  // interface.
  const mojo::AssociatedRemote<mojom::FrameBindingsControl>&
  GetFrameBindingsControl();

  // Resets the loading state. Following this call, the RenderFrameHost will be
  // in a non-loading state.
  void ResetLoadingState();

  // Returns the feature policy which should be enforced on this RenderFrame.
  const blink::FeaturePolicy* feature_policy() const {
    return feature_policy_.get();
  }

  void ClearFocusedElement();

  // Returns the PreviewsState of the last successful navigation
  // that made a network request. The PreviewsState is a bitmask of potentially
  // several Previews optimizations.
  blink::PreviewsState last_navigation_previews_state() const {
    return last_navigation_previews_state_;
  }

  bool has_focused_editable_element() const {
    return has_focused_editable_element_;
  }

  // Note: The methods for blocking / resuming / cancelling requests per
  // RenderFrameHost are deprecated and will not work in the network service,
  // please avoid using them.
  //
  // Causes all new requests for the root RenderFrameHost and its children to
  // be blocked (not being started) until ResumeBlockedRequestsForFrame is
  // called.
  void BlockRequestsForFrame();

  // Resumes any blocked request for the specified root RenderFrameHost and
  // child frame hosts.
  void ResumeBlockedRequestsForFrame();

  // Cancels any blocked request for the frame and its subframes.
  void CancelBlockedRequestsForFrame();

  // Binds a DevToolsAgent interface for debugging.
  void BindDevToolsAgent(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver);

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHost() override;
  service_manager::InterfaceProvider* GetJavaInterfaces() override;
#endif

  // Propagates the visibility state along the immediate local roots by calling
  // RenderWidgetHostViewChildFrame::Show()/Hide(). Calling this on a pending
  // or speculative RenderFrameHost (that has not committed) should be avoided.
  void SetVisibilityForChildViews(bool visible);

  const base::UnguessableToken& GetTopFrameToken();

  // Returns an unguessable token for this RFHI.  This provides a temporary way
  // to identify a RenderFrameHost that's compatible with IPC.  Else, one needs
  // to send pid + RoutingID, but one cannot send pid.  One can get it from the
  // channel, but this makes it much harder to get wrong.
  // Once media switches to mojo, we should be able to remove this in favor of
  // sending a mojo overlay factory.
  const base::UnguessableToken& GetOverlayRoutingToken() const {
    return frame_token_;
  }

  // Binds the receiver end of the InterfaceProvider interface through which
  // services provided by this RenderFrameHost are exposed to the corresponding
  // RenderFrame. The caller is responsible for plumbing the client end to the
  // renderer process.
  void BindInterfaceProviderReceiver(
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider_receiver);

  // Binds the receiver end of the BrowserInterfaceBroker interface through
  // which services provided by this RenderFrameHost are exposed to the
  // corresponding RenderFrame. The caller is responsible for plumbing the
  // client end to the renderer process.
  void BindBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>);

  // Binds the receiver end of the DomOperationControllerHost interface through
  // which services provided by this RenderFrameHost are exposed to the
  // corresponding RenderFrame. The caller is responsible for plumbing the
  // client end to the renderer process.
  void BindDomOperationControllerHostReceiver(
      mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
          receiver);

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::AssociatedReceiver<mojom::FrameHost>&
  frame_host_receiver_for_testing() {
    return frame_host_associated_receiver_;
  }

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>&
  local_frame_host_receiver_for_testing() {
    return local_frame_host_receiver_;
  }

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::AssociatedReceiver<blink::mojom::LocalMainFrameHost>&
  local_main_frame_host_receiver_for_testing() {
    return local_main_frame_host_receiver_;
  }

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>&
  browser_interface_broker_receiver_for_testing() {
    return broker_receiver_;
  }
  void SetKeepAliveTimeoutForTesting(base::TimeDelta timeout);

  network::mojom::WebSandboxFlags active_sandbox_flags() {
    return active_sandbox_flags_;
  }

  bool is_mhtml_document() { return is_mhtml_document_; }

  // Notifies the render frame that |frame_tree_node_| has had the sticky
  // user activation bit set for the first time.
  void DidReceiveFirstUserActivation();

  // Returns the current size for this frame.
  const base::Optional<gfx::Size>& frame_size() const { return frame_size_; }

  // Allow tests to override the timeout used to keep subframe processes alive
  // for unload handler processing.
  void SetSubframeUnloadTimeoutForTesting(const base::TimeDelta& timeout);

  // Called when the WebAudio AudioContext given by |audio_context_id| has
  // started (or stopped) playing audible audio.
  void AudioContextPlaybackStarted(int audio_context_id);
  void AudioContextPlaybackStopped(int audio_context_id);

  // Called when this RenderFrameHostImpl enters the BackForwardCache, the
  // document enters in a "Frozen" state where no Javascript can run.
  void DidEnterBackForwardCache();

  // Called when this RenderFrameHostImpl leaves the BackForwardCache. This
  // occurs immediately before a restored document is committed.
  void WillLeaveBackForwardCache();

  // Take ownership over the DidCommitProvisionalLoad_Params that
  // were last used to commit this navigation.
  // This is used by the BackForwardCache to re-commit when navigating to a
  // restored page.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  TakeLastCommitParams();

  // Start a timer that will evict this RenderFrameHost from the
  // BackForwardCache after time to live.
  void StartBackForwardCacheEvictionTimer();

  bool IsBackForwardCacheDisabled() const;

  // Prevents this frame (along with its parents/children) from being added to
  // the BackForwardCache. If the frame is already in the cache an eviction is
  // triggered.
  void DisableBackForwardCache(base::StringPiece reason);

  bool is_evicted_from_back_forward_cache() {
    return is_evicted_from_back_forward_cache_;
  }

  const std::set<std::string>& back_forward_cache_disabled_reasons() const {
    return back_forward_cache_disabled_reasons_;
  }

  // Prevents this frame to do a proactive BrowsingInstance swap (for all
  // navigations on this frame - cross-site and same-site).
  void DisableProactiveBrowsingInstanceSwapForTesting();

  bool HasTestDisabledProactiveBrowsingInstanceSwap() const {
    return has_test_disabled_proactive_browsing_instance_swap_;
  }

  void AddServiceWorkerContainerHost(
      const std::string& uuid,
      base::WeakPtr<ServiceWorkerContainerHost> host);
  void RemoveServiceWorkerContainerHost(const std::string& uuid);
  // Returns the last committed ServiceWorkerContainerHost of this frame.
  // The function is called on the UI thread, but the returned pointer can only
  // be dereferenced on the thread identified by
  // ServiceWorkerContext::GetCoreThreadId().
  base::WeakPtr<ServiceWorkerContainerHost> GetLastCommittedServiceWorkerHost();

  // Called to taint |this| so the pages which have requested MediaStream
  // (audio/video/etc capture stream) access would not enter BackForwardCache.
  void OnGrantedMediaStreamAccess();
  bool was_granted_media_access() { return was_granted_media_access_; }

  // Request a new NavigationClient interface from the renderer and returns the
  // ownership of the mojo::AssociatedRemote. This is intended for use by the
  // NavigationRequest. Only used with PerNavigationMojoInterface enabled.
  mojo::AssociatedRemote<mojom::NavigationClient>
  GetNavigationClientFromInterfaceProvider();

  // Called to signify the RenderFrameHostImpl that one of its ongoing
  // NavigationRequest's has been cancelled.
  void NavigationRequestCancelled(NavigationRequest* navigation_request);

  // Called on the main frame of a page embedded in a Portal when it is
  // activated. The frame has the option to adopt the previous page,
  // |predecessor|, as a portal. The activation can optionally include a message
  // |data| dispatched with the PortalActivateEvent.
  void OnPortalActivated(
      std::unique_ptr<Portal> predecessor,
      mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal,
      mojo::PendingAssociatedReceiver<blink::mojom::PortalClient>
          client_receiver,
      blink::TransferableMessage data,
      base::OnceCallback<void(blink::mojom::PortalActivateResult)> callback);

  // Called in tests that synthetically create portals but need them to be
  // properly associated with the owning RenderFrameHost.
  void OnPortalCreatedForTesting(std::unique_ptr<Portal> portal);

  // Look up a portal by its token (as received from the renderer process).
  Portal* FindPortalByToken(const blink::PortalToken& portal_token);

  // Return portals owned by |this|.
  std::vector<Portal*> GetPortals() const;

  // Called when a Portal needs to be destroyed.
  void DestroyPortal(Portal* portal);

  // Called on the main frame of a page embedded in a Portal to forward a
  // message from the host of a portal.
  void ForwardMessageFromHost(blink::TransferableMessage message,
                              const url::Origin& source_origin);

  // Returns true if the frame is embedded in a Portal.
  bool InsidePortal();

  bool ShouldVirtualKeyboardOverlayContent() const;
  void NotifyVirtualKeyboardOverlayRect(const gfx::Rect& keyboard_rect);

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

  // A CommitCallbackInterceptor is used to modify parameters for or cancel a
  // DidCommitNavigation call in tests.
  // WillProcessDidCommitNavigation will be run right after entering a
  // navigation callback and if returning false, will return straight away.
  class CommitCallbackInterceptor {
   public:
    CommitCallbackInterceptor() {}
    virtual ~CommitCallbackInterceptor() {}

    virtual bool WillProcessDidCommitNavigation(
        NavigationRequest* navigation_request,
        ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
        mojom::DidCommitProvisionalLoadInterfaceParamsPtr*
            interface_params) = 0;
  };

  // Sets the specified |interceptor|.
  void SetCommitCallbackInterceptorForTesting(
      CommitCallbackInterceptor* interceptor);

  // Posts a message from a frame in another process to the current renderer.
  void PostMessageEvent(
      const base::Optional<base::UnguessableToken>& source_token,
      const base::string16& source_origin,
      const base::string16& target_origin,
      blink::TransferableMessage message);

  // Requests to swap the current frame into the frame tree, replacing the
  // RenderFrameProxy it is associated with.
  void SwapIn();

  // Manual RTTI to ensure safe downcasts in tests.
  virtual bool IsTestRenderFrameHost() const;

  // Scheduler-relevant features this frame is using, for use in metrics.
  // See comments at |scheduler_tracked_features_|.
  uint64_t scheduler_tracked_features() const {
    return renderer_reported_scheduler_tracked_features_ |
           browser_reported_scheduler_tracked_features_;
  }

  // Returns a PrefetchedSignedExchangeCache which is attached to |this| iff
  // SignedExchangeSubresourcePrefetch feature or
  // SignedExchangePrefetchCacheForNavigations feature is enabled.
  scoped_refptr<PrefetchedSignedExchangeCache>
  EnsurePrefetchedSignedExchangeCache();

  // Clears the entries in the PrefetchedSignedExchangeCache if exists.
  void ClearPrefetchedSignedExchangeCache();

  // Creates a WebBundleHandleTracker from WebBundleHandles which are attached
  // |this| or the parent frame or the opener frame.
  std::unique_ptr<WebBundleHandleTracker> MaybeCreateWebBundleHandleTracker();

  // Adds |message| to the DevTools console only if it is unique (i.e. has not
  // been added to the console previously from this frame).
  virtual void AddUniqueMessageToConsole(
      blink::mojom::ConsoleMessageLevel level,
      const std::string& message);

  // Notify the scheduler that this frame used a feature which impacts the
  // scheduling policy (e.g. whether the frame can be frozen or put into the
  // back-forward cache).
  void OnSchedulerTrackedFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature feature);

  // Returns true if the frame is frozen.
  bool IsFrozen();

  void CreateAppCacheBackend(
      mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver);

  void GetAudioContextManager(
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver);

  void GetContactsManager(
      mojo::PendingReceiver<blink::mojom::ContactsManager> receiver);

  void GetFileSystemManager(
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver);

  void GetGeolocationService(
      mojo::PendingReceiver<blink::mojom::GeolocationService> receiver);

  void GetFontAccessManager(
      mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver);

  void GetNativeFileSystemManager(
      mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver);

#if !defined(OS_ANDROID)
  void GetHidService(mojo::PendingReceiver<blink::mojom::HidService> receiver);

  void BindSerialService(
      mojo::PendingReceiver<blink::mojom::SerialService> receiver);
#endif

  IdleManager* GetIdleManager();

  void BindIdleManager(
      mojo::PendingReceiver<blink::mojom::IdleManager> receiver);

  void GetPresentationService(
      mojo::PendingReceiver<blink::mojom::PresentationService> receiver);

  PresentationServiceImpl& GetPresentationServiceForTesting();

  void GetSpeechSynthesis(
      mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver);

  void CreateLockManager(
      mojo::PendingReceiver<blink::mojom::LockManager> receiver);

  void CreateIDBFactory(
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver);

  void GetSensorProvider(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver);

  void CreatePermissionService(
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver);

  void CreatePaymentManager(
      mojo::PendingReceiver<payments::mojom::PaymentManager> receiver);

  void CreateWebBluetoothService(
      mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver);

  void GetAuthenticator(
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver);

  void GetVirtualAuthenticatorManager(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver);

  void GetPushMessaging(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver);

  void CreateDedicatedWorkerHostFactory(
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);

  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);

  void CreateNotificationService(
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver);

  void CreateInstalledAppProvider(
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);

#if defined(OS_ANDROID)
  void BindNFCReceiver(mojo::PendingReceiver<device::mojom::NFC> receiver);
#endif

  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

  void BindInputInjectorReceiver(
      mojo::PendingReceiver<mojom::InputInjector> receiver);

  void BindSmsReceiverReceiver(
      mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver);

  void BindRestrictedCookieManager(
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver);

  // Requires the following preconditions, reporting a bad message otherwise.
  //
  // 1. This frame's top-frame origin must be potentially trustworthy and
  // have scheme HTTP or HTTPS. (See network::SuitableTrustTokenOrigin's class
  // comment for the rationale.)
  //
  // 2. Trust Tokens must be enabled (network::features::kTrustTokens).
  //
  // 3. This frame's origin must be potentially trustworthy.
  void BindHasTrustTokensAnswerer(
      mojo::PendingReceiver<network::mojom::HasTrustTokensAnswerer> receiver);

  // Creates connections to WebUSB interfaces bound to this frame.
  void CreateWebUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

  void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver);

  void BindMediaInterfaceFactoryReceiver(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);

  void BindMediaMetricsProviderReceiver(
      mojo::PendingReceiver<media::mojom::MediaMetricsProvider> receiver);

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  void BindMediaRemoterFactoryReceiver(
      mojo::PendingReceiver<media::mojom::RemoterFactory> receiver);
#endif

  void CreateAudioInputStreamFactory(
      mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
          receiver);

  void CreateAudioOutputStreamFactory(
      mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
          receiver);

  void GetFeatureObserver(
      mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver);

  void BindScreenEnumerationReceiver(
      mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver);

  // https://mikewest.github.io/corpp/#initialize-embedder-policy-for-global
  const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy()
      const {
    return cross_origin_embedder_policy_;
  }
  void set_cross_origin_embedder_policy(
      network::CrossOriginEmbedderPolicy policy) {
    cross_origin_embedder_policy_ = policy;
  }
  CrossOriginEmbedderPolicyReporter* coep_reporter() {
    return coep_reporter_.get();
  }
  void set_coop_reporter(
      std::unique_ptr<CrossOriginOpenerPolicyReporter>&& reporter) {
    coop_reporter_ = std::move(reporter);
  }

  // Semi-formal definition of COOP:
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
  network::CrossOriginOpenerPolicy cross_origin_opener_policy() const {
    return cross_origin_opener_policy_;
  }
  void set_cross_origin_opener_policy_for_testing(
      const network::CrossOriginOpenerPolicy& cross_origin_opener_policy) {
    cross_origin_opener_policy_ = cross_origin_opener_policy;
  }
  CrossOriginOpenerPolicyReporter* coop_reporter() {
    return coop_reporter_.get();
  }
  int virtual_browsing_context_group() const {
    return virtual_browsing_context_group_;
  }

  const network::mojom::ClientSecurityStatePtr&
  last_committed_client_security_state() const {
    return last_committed_client_security_state_;
  }

  const network::mojom::ContentSecurityPolicy* required_csp() {
    return required_csp_.get();
  }

  // This function mimics DidCommitProvisionalLoad for navigations served from
  // the back-forward cache.
  void DidCommitBackForwardCacheNavigation(
      NavigationRequest* committing_navigation_request,
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params);

  // Whether there's any "unload" event handlers registered on this
  // RenderFrameHost or subframes that share the same SiteInstance as this
  // RenderFrameHost.
  bool UnloadHandlerExistsInSameSiteInstanceSubtree();

  bool has_committed_any_navigation() const {
    return has_committed_any_navigation_;
  }

  // Return true if the process this RenderFrameHost is using has crashed and we
  // are replacing RenderFrameHosts for crashed frames rather than reusing them.
  //
  // This is not exactly the opposite of IsRenderFrameLive().
  // IsRenderFrameLive() is false when the RenderProcess died, but it is also
  // false when it hasn't been initialized.
  bool must_be_replaced() const { return must_be_replaced_; }
  // Resets the must_be_replaced after the RFH has been reinitialized. Do not
  // add any more usages of this.
  // TODO(https://crbug.com/936696): Remove this.
  void reset_must_be_replaced() { must_be_replaced_ = false; }

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateCrossOriginPrefetchLoaderFactoryBundle();

  const AppCacheNavigationHandle* GetAppCacheNavigationHandle() const {
    return appcache_handle_.get();
  }

  // Returns the BackForwardCacheMetrics associated with the last
  // NavigationEntry this RenderFrameHostImpl committed.
  BackForwardCacheMetrics* GetBackForwardCacheMetrics();

  const std::string& GetEncoding() const { return canonical_encoding_; }

  // Returns a base salt used to generate frame-specific IDs for media-device
  // enumerations.
  const std::string& GetMediaDeviceIDSaltBase() const {
    return media_device_id_salt_base_;
  }

  // Returns the reason why the BrowsingInstance wasn't swapped for the last
  // navigation inside this frame.
  // TODO(crbug.com/1026101): Remove after the investigation.
  base::Optional<ShouldSwapBrowsingInstance>
  browsing_instance_not_swapped_reason() const {
    return browsing_instance_not_swapped_reason_;
  }

  void set_browsing_instance_not_swapped_reason(
      ShouldSwapBrowsingInstance reason) {
    browsing_instance_not_swapped_reason_ = reason;
  }

  // Returns the parent RenderFrameHost, potentially going through nested
  // WebContents. Returns nullptr for top-level RenderFrameHosts in topmost
  // WebContents.
  RenderFrameHostImpl* ParentOrOuterDelegateFrame();

  void SetIsOuterDelegateFrame(bool is_outer_frame) {
    is_outer_delegate_frame_ = is_outer_frame;
  }
  bool IsOuterDelegateFrame() { return is_outer_delegate_frame_; }

  bool DocumentUsedWebOTP() override;

  scoped_refptr<WebAuthRequestSecurityChecker>
  GetWebAuthRequestSecurityChecker();

  base::WeakPtr<RenderFrameHostImpl> GetWeakPtr();

  // blink::mojom::LocalFrameHost
  void EnterFullscreen(blink::mojom::FullscreenOptionsPtr options,
                       EnterFullscreenCallback callback) override;
  void ExitFullscreen() override;
  void FullscreenStateChanged(bool is_fullscreen) override;
  void RegisterProtocolHandler(const std::string& scheme,
                               const GURL& url,
                               const base::string16& title,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(const std::string& scheme,
                                 const GURL& url,
                                 bool user_gesture) override;
  void DidDisplayInsecureContent() override;
  void DidContainInsecureFormAction() override;
  void DocumentAvailableInMainFrame(bool uses_temporary_zoom_level) override;
  void SetNeedsOcclusionTracking(bool needs_tracking) override;
  void SetVirtualKeyboardOverlayPolicy(bool vk_overlays_content) override;
  void EvictFromBackForwardCache() override;
  void VisibilityChanged(blink::mojom::FrameVisibility) override;
  void DidChangeThemeColor(const base::Optional<SkColor>& theme_color) override;
  void DidChangeBackgroundColor(const SkColor& background_color) override;
  void DidFailLoadWithError(const GURL& url, int32_t error_code) override;
  void DidFocusFrame() override;
  void DidCallFocus() override;
  void DidAddContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyPtr> policies) override;
  void EnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy) override;
  void EnforceInsecureNavigationsSet(const std::vector<uint32_t>& set) override;
  void DidChangeActiveSchedulerTrackedFeatures(uint64_t features_mask) override;
  void SuddenTerminationDisablerChanged(
      bool present,
      blink::mojom::SuddenTerminationDisablerType disabler_type) override;
  void HadStickyUserActivationBeforeNavigationChanged(bool value) override;
  void ScrollRectToVisibleInParentFrame(
      const gfx::Rect& rect_to_scroll,
      blink::mojom::ScrollIntoViewParamsPtr params) override;
  void BubbleLogicalScrollInParentFrame(
      blink::mojom::ScrollDirection direction,
      ui::ScrollGranularity granularity) override;
  void DidAccessInitialDocument() override;
  void DidBlockNavigation(
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  void DidChangeLoadProgress(double load_progress) override;
  void DidFinishLoad(const GURL& validated_url) override;
  void DispatchLoad() override;
  void GoToEntryAtOffset(int32_t offset, bool has_user_gesture) override;
  void RenderFallbackContentInParentProcess() override;
  void UpdateTitle(const base::Optional<::base::string16>& title,
                   base::i18n::TextDirection title_direction) override;
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override;
  void HandleAccessibilityFindInPageResult(
      blink::mojom::FindInPageResultAXParamsPtr params) override;
  void HandleAccessibilityFindInPageTermination() override;
  void DocumentOnLoadCompleted() override;
  void ForwardResourceTimingToParent(
      blink::mojom::ResourceTimingInfoPtr timing) override;
  void DidFinishDocumentLoad() override;
  void RunModalAlertDialog(const base::string16& alert_message,
                           RunModalAlertDialogCallback callback) override;
  void RunModalConfirmDialog(const base::string16& alert_message,
                             RunModalConfirmDialogCallback callback) override;
  void RunModalPromptDialog(const base::string16& alert_message,
                            const base::string16& default_value,
                            RunModalPromptDialogCallback callback) override;
  void RunBeforeUnloadConfirm(bool is_reload,
                              RunBeforeUnloadConfirmCallback callback) override;
  void UpdateFaviconURL(
      std::vector<blink::mojom::FaviconURLPtr> favicon_urls) override;
  void DownloadURL(blink::mojom::DownloadURLParamsPtr params) override;
  void FocusedElementChanged(bool is_editable_element,
                             const gfx::Rect& bounds_in_frame_widget,
                             blink::mojom::FocusType focus_type) override;
  void TextSelectionChanged(const base::string16& text,
                            uint32_t offset,
                            const gfx::Range& range) override;
  void ShowPopupMenu(
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
  void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& http_method,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination) override;
  void DidChangeFrameOwnerProperties(
      const base::UnguessableToken& child_frame_token,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties) override;
  void DidChangeOpener(
      const base::Optional<base::UnguessableToken>& opener_frame) override;
  void DidChangeCSPAttribute(
      const base::UnguessableToken& child_frame_token,
      network::mojom::ContentSecurityPolicyPtr parsed_csp_attribute) override;
  void DidChangeFramePolicy(const base::UnguessableToken& child_frame_token,
                            const blink::FramePolicy& frame_policy) override;
  void CapturePaintPreviewOfSubframe(
      const gfx::Rect& clip_rect,
      const base::UnguessableToken& guid) override;
  void Detach() override;

  // blink::LocalMainFrameHost overrides:
  void ScaleFactorChanged(float scale) override;
  void ContentsPreferredSizeChanged(const gfx::Size& pref_size) override;
  void TextAutosizerPageInfoChanged(
      blink::mojom::TextAutosizerPageInfoPtr page_info) override;
  void FocusPage() override;
  void UpdateTargetURL(const GURL& url,
                       blink::mojom::LocalMainFrameHost::UpdateTargetURLCallback
                           callback) override;

  void ReportNoBinderForInterface(const std::string& error);

  // Returns true if this object has any NavigationRequests matching |origin|.
  // Since this function is used to find existing committed/committing origins
  // that have not opted-in to isolation, and since any calls to this function
  // will be initiated by a NavigationRequest that is itself requesting opt-in
  // isolation, |navigation_request_to_exclude| allows that request to exclude
  // itself from consideration.
  bool HasCommittingNavigationRequestForOrigin(
      const url::Origin& origin,
      NavigationRequest* navigation_request_to_exclude);

  // Force the RenderFrameHost to be left in pending deletion state instead of
  // being actually deleted after navigating away:
  // - Force waiting for unload handler result regardless of whether an
  //   unload handler is present or not.
  // - Disable unload timeout monitor.
  // - Ignore any OnUnloadACK sent by the renderer process.
  void DoNotDeleteForTesting();

  // This method will unset the flag |do_not_delete_for_testing_| to resume
  // deletion on the RenderFrameHost. Deletion will only be triggered if
  // RenderFrameHostImpl::Detach() is called for the RenderFrameHost. This is a
  // counterpart for DoNotDeleteForTesting() which sets the flag
  // |do_not_delete_for_testing_|.
  void ResumeDeletionForTesting();

  // This method will detach forcely RenderFrameHost with setting the states,
  // |do_not_delete_for_testing_| and |detach_state_|, to resume deletion on
  // the RenderFrameHost.
  void DetachForTesting();

  // Document-associated data. This is cleared whenever a new document is hosted
  // by this RenderFrameHost. Please refer to the description at
  // content/public/browser/render_document_host_user_data.h for more details.
  base::SupportsUserData::Data* GetRenderDocumentHostUserData(
      const void* key) const {
    return document_associated_data_.GetUserData(key);
  }

  void SetRenderDocumentHostUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) {
    document_associated_data_.SetUserData(key, std::move(data));
  }

  void RemoveRenderDocumentHostUserData(const void* key) {
    document_associated_data_.RemoveUserData(key);
  }

  // Called when we commit speculative RFH early due to not having an alive
  // current frame. This happens when the renderer crashes before navigating to
  // a new URL using speculative RenderFrameHost.
  // TODO(https://crbug.com/1072817): Undo this plumbing after removing the
  // early post-crash CommitPending() call.
  void OnCommittedSpeculativeBeforeNavigationCommit() {
    committed_speculative_rfh_before_navigation_commit_ = true;
  }

  // Returns the child RenderFrameHostImpl if |child_frame_routing_id| is an
  // immediate child of this FrameTreeNode. |child_frame_routing_id| is
  // considered untrusted, so the renderer process is killed if it refers to a
  // RenderFrameHostImpl that is not a child of this node.
  RenderFrameHostImpl* FindAndVerifyChild(int32_t child_frame_routing_id,
                                          bad_message::BadMessageReason reason);

  // Returns the child RenderFrameHostImpl if |child_frame_token| is an
  // immediate child of this FrameTreeNode. |child_frame_token| is considered
  // untrusted, so the renderer process is killed if it refers to a
  // RenderFrameHostImpl that is not a child of this node.
  RenderFrameHostImpl* FindAndVerifyChild(
      const base::UnguessableToken& child_frame_token,
      bad_message::BadMessageReason reason);

  // Whether we should run the pagehide/visibilitychange handlers of the
  // RenderFrameHost we're navigating away from (|old_frame_host|) during the
  // commit to a new RenderFrameHost (this RenderFrameHost). Should only return
  // true when we're doing a same-site navigation and we did a proactive
  // BrowsingInstance swap but we're reusing the old page's renderer process.
  // We should run pagehide and visibilitychange handlers of the old page during
  // the commit of the new main frame in those cases because in other same-site
  // navigations we will run those handlers before the new page finished
  // committing. Note that unload handlers will still run after the new page
  // finished committing. Ideally we would run unload handlers alongside
  // pagehide and visibilitychange handlers at commit time too, but we'd need to
  // actually unload/freeze the page in that case which is more complex.
  // TODO(crbug.com/1110744): Support unload-in-commit.
  bool ShouldDispatchPagehideAndVisibilitychangeDuringCommit(
      RenderFrameHostImpl* old_frame_host,
      const UrlInfo& dest_url_info);

  mojo::PendingRemote<network::mojom::CookieAccessObserver>
  CreateCookieAccessObserver();

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(
      network::mojom::CookieAccessDetailsPtr details) override;

  void GetSavableResourceLinksFromRenderer();

  // Helper for checking if a navigation to an error page should be excluded
  // from CanAccessDataForOrigin and/or CanCommitOriginAndUrl security checks.
  //
  // It is allowed for |navigation_request| to be null - for example when
  // committing a same-document navigation.
  //
  // The optional |should_commit_unreachable_url| will be set to |true| if the
  // caller should verify that FrameHostMsg_DidCommitProvisionalLoad_Params'
  // url_is_unreachable is |true|.
  bool ShouldBypassSecurityChecksForErrorPage(
      NavigationRequest* navigation_request,
      bool* should_commit_unreachable_url = nullptr);

  // Explicitly allow the use of an audio output device in this render frame.
  // When called with a hashed device id string the renderer will be allowed to
  // use the associated device for audio output until this method is called
  // again with a different hashed device id or the origin changes. To remove
  // this permission, this method may be called with the empty string.
  void SetAudioOutputDeviceIdForGlobalMediaControls(
      std::string hashed_device_id);

  // Returns a filter that should be associated with all AssociatedReceivers for
  // this frame. |interface_name| is used for logging purposes and must be valid
  // for the entire program duration.
  std::unique_ptr<mojo::MessageFilter> CreateMessageFilterForAssociatedReceiver(
      const char* interface_name);

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  // |lifecycle_state_| can either be kActive or kSpeculative during
  // RenderFrameHostImpl creation.
  RenderFrameHostImpl(SiteInstance* site_instance,
                      scoped_refptr<RenderViewHostImpl> render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int32_t routing_id,
                      const base::UnguessableToken& frame_token,
                      bool renderer_initiated_creation,
                      LifecycleState lifecycle_state);

  // The SendCommit* functions below are wrappers for commit calls
  // made to mojom::NavigationClient.
  // These exist to be overridden in tests to retain mojo callbacks.
  // Note: |navigation_id| is used in test overrides, but is unused otherwise.
  virtual void SendCommitNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token);
  virtual void SendCommitFailedNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int32_t error_code,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories);

  // The Build*Callback functions below are responsible for building the
  // callbacks for either successful or failed commits.
  // Protected because they need to be called from test overrides.
  mojom::NavigationClient::CommitNavigationCallback
  BuildCommitNavigationCallback(NavigationRequest* navigation_request);
  mojom::NavigationClient::CommitFailedNavigationCallback
  BuildCommitFailedNavigationCallback(NavigationRequest* navigation_request);

  // Protected / virtual so it can be overridden by tests.
  virtual void SendBeforeUnload(bool is_reload,
                                base::WeakPtr<RenderFrameHostImpl> impl);

 private:
  friend class RenderFrameHostFeaturePolicyTest;
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;
  friend class TextInputTestLocalFrame;
  friend class WebContentsSplitCacheBrowserTest;
  friend class RenderFrameHostManagerUnloadBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(NavigatorTest, TwoNavigationsRacingCommit);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           SubframeShowsDialogWhenMainFrameNavigates);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           TimerNotRestartedBySecondDialog);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           ComputeSiteForCookiesParentNavigatedAway);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           CheckIsCurrentBeforeAndAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           HasCommittedAnyNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest, GetUkmSourceIds);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CreateRenderViewAfterProcessKillAndClosedProxy);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, DontSelectInvalidFiles);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreSubframeFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RenderViewInitAfterNewProxyAndProcessKill);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           UnloadPushStateOnCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           WebUIJavascriptDisallowedAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, LastCommittedOrigin);
  FRIEND_TEST_ALL_PREFIXES(
      RenderFrameHostManagerUnloadBrowserTest,
      PendingDeleteRFHProcessShutdownDoesNotRemoveSubframes);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrashSubframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, FindImmediateLocalRoots);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostIsNotReusedAfterDelayedUnloadACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostStaysActiveWithLateUnloadACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LoadEventForwardingWhilePendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ContextMenuAfterCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ActiveSandboxFlagsRetainedAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LastCommittedURLRetainedAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderFrameProxyNotRecreatedDuringProcessShutdown);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadACKArrivesPriorToProcessShutdownRequest);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderViewHost);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           FullscreenAfterFrameUnload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, UnloadHandlerSubframes);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, Unload_ABAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadNestedPendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, PartialUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           PendingDeletionCheckCompletedOnSubtree);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           DetachedIframeUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionABC);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           CommittedOriginIncompatibleWithOriginLock);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerSameProcess);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerCrossProcess);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessSSLBrowserTest,
                           UnloadHandlersArePowerful);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessSSLBrowserTest,
                           UnloadHandlersArePowerfulGrandChild);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplTest, ExpectedMainWorldOrigin);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderWidgetHost);
  FRIEND_TEST_ALL_PREFIXES(RenderDocumentHostUserDataTest,
                           CheckInPendingDeletionState);

  class DroppedInterfaceRequestLogger;

  RenderFrameHostImpl* FindAndVerifyChildInternal(
      RenderFrameHostOrProxy child_frame_or_proxy,
      bad_message::BadMessageReason reason);

  // Update the RenderProcessHost priority when a navigation occurs.
  void UpdateRenderProcessHostFramePriorities();

  // IPC Message handlers.
  void OnUnloadACK();
  void OnContextMenu(const UntrustworthyContextMenuParams& params);
  void OnForwardResourceTimingToParent(
      const ResourceTimingInfo& resource_timing);
  void OnSetNeedsOcclusionTracking(bool needs_tracking);
  void OnSaveImageFromDataURL(const std::string& url_str);

  // Computes the IsolationInfo for both navigations and subresources.
  //
  // For navigations, |frame_origin| is the origin being navigated to. For
  // subresources, |frame_origin| is the value of |last_committed_origin_|.
  net::IsolationInfo ComputeIsolationInfoInternal(
      const url::Origin& frame_origin,
      net::IsolationInfo::RedirectMode redirect_mode) const;

#if defined(OS_ANDROID)
  void ForwardGetInterfaceToRenderFrame(const std::string& interface_name,
                                        mojo::ScopedMessagePipeHandle pipe);
#endif

  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       CreateNewWindowCallback callback) override;
  void CreatePortal(
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> pending_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
      CreatePortalCallback callback) override;
  void AdoptPortal(const blink::PortalToken& portal_token,
                   AdoptPortalCallback callback) override;
  void CreateNewWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
      CreateNewWidgetCallback callback) override;
  void CreateNewFullscreenWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
      CreateNewFullscreenWidgetCallback callback) override;
  void IssueKeepAliveHandle(
      mojo::PendingReceiver<mojom::KeepAliveHandle> receiver) override;
  void DidCommitProvisionalLoad(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params)
      override;

  // This function mimics DidCommitProvisionalLoad but is a direct mojo
  // callback from NavigationClient::CommitNavigation.
  // This only used when PerNavigationMojoInterface is enabled, and will
  // replace DidCommitProvisionalLoad in the long run.
  void DidCommitPerNavigationMojoInterfaceNavigation(
      NavigationRequest* committing_navigation_request,
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

  void DidCommitSameDocumentNavigation(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params)
      override;
  void BeginNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator) override;
  void SubresourceResponseStarted(const GURL& url,
                                  net::CertStatus cert_status) override;
  void ResourceLoadComplete(
      blink::mojom::ResourceLoadInfoPtr resource_load_info) override;
  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override;
  void DidSetFramePolicyHeaders(
      network::mojom::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& feature_policy_header,
      const blink::DocumentPolicyFeatureState& document_policy_header) override;
  void CancelInitialHistoryLoad() override;
  void UpdateEncoding(const std::string& encoding) override;
  void FrameSizeChanged(const gfx::Size& frame_size) override;
  void DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void ShowCreatedWindow(int32_t pending_widget_routing_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) override;
  void UpdateState(const PageState& state) override;
  void OpenURL(mojom::OpenURLParamsPtr params) override;
  void DidStopLoading() override;

#if defined(OS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override;
#endif

  // mojom::RenderAccessibilityHost:
  void HandleAXEvents(const std::vector<ui::AXTreeUpdate>& updates,
                      const std::vector<ui::AXEvent>& events,
                      int32_t reset_token,
                      HandleAXEventsCallback callback) override;
  void HandleAXLocationChanges(
      std::vector<mojom::LocationChangesPtr> changes) override;

  // mojom::DomAutomationControllerHost:
  void DomOperationResponse(const std::string& json_string) override;

  // network::mojom::CookieAccessObserver
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

  // Resets any waiting state of this RenderFrameHost that is no longer
  // relevant.
  void ResetWaitingState();

  // Returns whether the given origin and URL is allowed to commit in the
  // current RenderFrameHost. The |url| is used to ensure it matches the origin
  // in cases where it is applicable. This is a more conservative check than
  // RenderProcessHost::FilterURL, since it will be used to kill processes that
  // commit unauthorized origins.
  CanCommitStatus CanCommitOriginAndUrl(const url::Origin& origin,
                                        const GURL& url);

  // Asserts that the given RenderFrameHostImpl is part of the same browser
  // context (and crashes if not), then returns whether the given frame is
  // part of the same site instance.
  bool IsSameSiteInstance(RenderFrameHostImpl* other_render_frame_host);

  // Returns whether the current RenderProcessHost has read access to all the
  // files reported in |state|.
  bool CanAccessFilesOfPageState(const PageState& state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |validated_state|.  It is important that the PageState has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromPageState(const PageState& validated_state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |body|.  It is important that the ResourceRequestBody has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromResourceRequestBody(
      const network::ResourceRequestBody& body);

  void UpdatePermissionsForNavigation(
      const mojom::CommonNavigationParams& common_params,
      const mojom::CommitNavigationParams& commit_params);

  // Returns true if there is an active transient fullscreen allowance for the
  // Window Placement feature (i.e. on screen configuration changes).
  bool WindowPlacementAllowsFullscreen();

  // Returns the latest NavigationRequest that has resulted in sending a Commit
  // IPC to the renderer process that hasn't yet been acked by the DidCommit IPC
  // from the renderer process.  Returns null if no such NavigationRequest
  // exists.
  NavigationRequest* FindLatestNavigationRequestThatIsStillCommitting();

  // Extracts all the |out_...| values from either the |navigation_request| (if
  // present) or from |this| (if |navigation_request| is null).
  void ExtractFactoryParamsFromNavigationRequestOrLastCommittedNavigation(
      NavigationRequest* navigation_request,
      url::Origin* out_main_world_origin,
      network::mojom::ClientSecurityStatePtr* out_client_security_state,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>*
          coep_reporter_pending_remote,
      network::mojom::TrustTokenRedemptionPolicy*
          out_trust_token_redemption_policy);

  // Creates URLLoaderFactoryParams for main world of |this|, either based on
  // the |navigation_request|, or (if |navigation_request| is null) on the last
  // committed navigation.
  network::mojom::URLLoaderFactoryParamsPtr
  CreateURLLoaderFactoryParamsForMainWorld(
      NavigationRequest* navigation_request,
      base::StringPiece debug_tag);

  // Creates a Network Service-backed factory from appropriate |NetworkContext|
  // and sets a connection error handler to trigger
  // |OnNetworkServiceConnectionError()| if the factory is out-of-process.  If
  // this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.
  bool CreateNetworkServiceDefaultFactoryAndObserve(
      network::mojom::URLLoaderFactoryParamsPtr params,
      base::UkmSourceId ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);
  bool CreateNetworkServiceDefaultFactoryInternal(
      network::mojom::URLLoaderFactoryParamsPtr params,
      base::UkmSourceId ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);

  // Lets ContentBrowserClient and devtools_instrumentation wrap the subresource
  // factories before they are sent to a renderer process.
  void WillCreateURLLoaderFactory(
      const url::Origin& request_initiator,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      base::UkmSourceId ukm_source_id,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client = nullptr,
      bool* bypass_redirect_checks = nullptr,
      bool* disable_secure_dns = nullptr,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override = nullptr);

  // Returns true if the ExecuteJavaScript() API can be used on this host.
  bool CanExecuteJavaScript();

  // Returns the AXTreeID of the parent when the current frame is a child frame
  // (i.e. not a main frame) or when it's an embedded browser plugin guest, or
  // ui::AXTreeIDUnknown() otherwise.
  ui::AXTreeID GetParentAXTreeID();

  // Returns the AXTreeID of the currently focused frame in the frame tree if
  // the current frame is the root frame, or ui::AXTreeIDUnknown otherwise.
  ui::AXTreeID GetFocusedAXTreeID();

  // Returns the AXTreeData associated to the current frame, ensuring that the
  // AXTreeIDs values for the current, parent and focused frames are up to date.
  ui::AXTreeData GetAXTreeData();

  // Callback in response to an accessibility hit test triggered by
  // AccessibilityHitTest.
  void AccessibilityHitTestCallback(
      int action_request_id,
      ax::mojom::Event event_to_fire,
      base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                              int hit_node_id)> opt_callback,
      mojom::HitTestResponsePtr hit_test_response);

  // Callback that will be called as a response to the call to the method
  // content::mojom::RenderAccessibility::SnapshotAccessibilityTree(). The
  // |callback| passed will be invoked after the renderer has responded with a
  // standalone snapshot of the accessibility tree as |snapshot|.
  void RequestAXTreeSnapshotCallback(AXTreeSnapshotCallback callback,
                                     const ui::AXTreeUpdate& snapshot);

  // Callback that will be called as a response to the call to the method
  // blink::mojom::LocalFrame::GetSavableResourceLinks(). The |reply| passed
  // will be a nullptr when the url is not the savable URLs or valid.
  void GetSavableResourceLinksCallback(
      blink::mojom::GetSavableResourceLinksReplyPtr reply);

  // Returns the RenderWidgetHostView used for accessibility. For subframes,
  // this function will return the platform view on the main frame; for main
  // frames, it will return the current frame's view.
  RenderWidgetHostViewBase* GetViewForAccessibility();

  // Returns a raw pointer to the Web Bluetooth Service owned by the frame. Used
  // for testing purposes only (see |TestRenderFrameHost|).
  WebBluetoothServiceImpl* GetWebBluetoothServiceForTesting();

  // Deletes the Web Bluetooth Service owned by the frame.
  void DeleteWebBluetoothService(
      WebBluetoothServiceImpl* web_bluetooth_service);

#if !defined(OS_ANDROID)
  void BindAuthenticatorReceiver(
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver);
#endif

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Allows tests to disable the unload event timer to simulate bugs that
  // happen before it fires (to avoid flakiness).
  void DisableUnloadTimerForTesting();

  // Creates a NavigationRequest to use for commit. This should only be used
  // when no appropriate NavigationRequest has been found.
  std::unique_ptr<NavigationRequest> CreateNavigationRequestForCommit(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool is_same_document);

  // Whether the |request| corresponds to a navigation to the pending
  // NavigationEntry. This is used at commit time, when the NavigationRequest
  // does not match the data sent by the renderer to re-create a
  // NavigationRequest and associate it with the pending NavigationEntry if
  // needed.
  // TODO(clamy): We should handle the mismatches gracefully without deleting
  // the NavigationRequest and having to re-create one.
  bool NavigationRequestWasIntendedForPendingEntry(
      NavigationRequest* request,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool same_document);

  // Helper to process the beforeunload completion callback. |proceed| indicates
  // whether the navigation or tab close should be allowed to proceed.  If
  // |treat_as_final_completion_callback| is true, the frame should stop waiting
  // for any further completion callbacks from subframes. Completion callbacks
  // invoked from the renderer set |treat_as_final_completion_callback| to
  // false, whereas a beforeunload timeout sets it to true.
  void ProcessBeforeUnloadCompleted(
      bool proceed,
      bool treat_as_final_completion_callback,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);

  // Find the frame that triggered the beforeunload handler to run in this
  // frame, which might be the frame itself or its ancestor.  This will
  // return the frame that is navigating, or the main frame if beforeunload was
  // triggered by closing the current tab.  It will return null if no
  // beforeunload is currently in progress.
  RenderFrameHostImpl* GetBeforeUnloadInitiator();

  // Called when a particular frame finishes running a beforeunload handler,
  // possibly as part of processing beforeunload for an ancestor frame. In
  // that case, this is called on the ancestor frame that is navigating or
  // closing, and |frame| indicates which beforeunload completion callback has
  // been invoked on. If a beforeunload timeout occurred,
  // |treat_as_final_completion_callback| is set to true.
  // |is_frame_being_destroyed| is set to true if this was called as part of
  // destroying |frame|.
  void ProcessBeforeUnloadCompletedFromFrame(
      bool proceed,
      bool treat_as_final_completion_callback,
      RenderFrameHostImpl* frame,
      bool is_frame_being_destroyed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);

  // Helper function to check whether the current frame and its subframes need
  // to run beforeunload and, if |send_ipc| is true, send all the necessary
  // IPCs for this frame's subtree. If |send_ipc| is false, this only checks
  // whether beforeunload is needed and returns the answer.  |subframes_only|
  // indicates whether to only check subframes of the current frame, and skip
  // the current frame itself.
  bool CheckOrDispatchBeforeUnloadForSubtree(bool subframes_only,
                                             bool send_ipc,
                                             bool is_reload);

  // Called by |beforeunload_timeout_| when the beforeunload timeout fires.
  void BeforeUnloadTimeout();

  // Update this frame's last committed origin.
  void SetLastCommittedOrigin(const url::Origin& origin);

  // Set the |last_committed_origin_|, |isolation_info_|, and |feature_policy_|
  // of |this| frame, inheriting the origin from |new_frame_creator| as
  // appropriate (e.g. depending on whether |this| frame should be sandboxed /
  // should have an opaque origin instead).
  void SetOriginDependentStateOfNewFrame(const url::Origin& new_frame_creator);

  // Called when a navigation commits successfully to |url|. This will update
  // |last_committed_site_info_| with the SiteInfo corresponding to |url|.
  // Note that this will recompute the SiteInfo from |url| rather than using
  // GetSiteInstance()->GetSiteInfo(), so that |last_committed_site_info_| is
  // always meaningful: e.g., without site isolation, b.com could commit in a
  // SiteInstance for a.com, but this function will still compute the last
  // committed SiteInfo as b.com.  For example, this can be used to track which
  // sites have committed in which process.
  void SetLastCommittedSiteInfo(const GURL& url);

  // Clears any existing policy and constructs a new policy for this frame,
  // based on its parent frame.
  void ResetFeaturePolicy();

  // TODO(ekaramad): One major purpose behind the API is to traverse the frame
  // tree top-down to visit the  RenderWidgetHostViews of interest in the most
  // efficient way. We might want to revisit this API, remove it from RFHImpl,
  // and perhaps consolidate it with some of the existing ones such as
  // WebContentsImpl::GetRenderWidgetHostViewsInTree() into a new more
  // appropriate API for dealing with (virtual) RenderWidgetHost(View) tree.
  // (see https://crbug.com/754726).
  // Runs |callback| for all the local roots immediately under this frame, i.e.
  // local roots which are under this frame and their first ancestor which is a
  // local root is either this frame or this frame's local root. For instance,
  // in a frame tree such as:
  //                    A0                   //
  //                 /  |   \                //
  //                B   A1   E               //
  //               /   /  \   \              //
  //              D  A2    C   F             //
  // RFHs at nodes B, E, D, C, and F are all local roots in the given frame tree
  // under the root at A0, but only B, C, and E are considered immediate local
  // roots of A0. Note that this will exclude any speculative or pending RFHs.
  void ForEachImmediateLocalRoot(
      const base::RepeatingCallback<void(RenderFrameHostImpl*)>& callback);

  // Lazily initializes and returns the mojom::FrameNavigationControl interface
  // for this frame. May be overridden by friend subclasses for e.g. tests which
  // wish to intercept outgoing navigation control messages.
  virtual mojom::FrameNavigationControl* GetNavigationControl();

  // Utility function used to validate potentially harmful parameters sent by
  // the renderer during the commit notification.
  // A return value of true means that the commit should proceed.
  bool ValidateDidCommitParams(
      NavigationRequest* navigation_request,
      FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      bool is_same_document_navigation);

  // Updates the site url if the navigation was successful and the page is not
  // an interstitial.
  void UpdateSiteURL(const GURL& url, bool url_is_unreachable);

  // The actual implementation of DidCommitProvisionalLoad and
  // DidCommitPerNavigationMojoInterfaceNavigation.
  void DidCommitNavigation(
      std::unique_ptr<NavigationRequest> committing_navigation_request,
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

  // Called when we receive the confirmation that a navigation committed in the
  // renderer. Used by both DidCommitSameDocumentNavigation and
  // DidCommitNavigation.
  // Returns true if the navigation did commit properly, false if the commit
  // state should be restored to its pre-commit value.
  bool DidCommitNavigationInternal(
      std::unique_ptr<NavigationRequest> navigation_request,
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      bool is_same_document_navigation);

  // Called when we received the confirmation a new document committed in the
  // renderer. It was created from the |navigation|.
  void DidCommitNewDocument(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      NavigationRequest* navigation);

  // Called by the renderer process when it is done processing a same-document
  // commit request.
  void OnSameDocumentCommitProcessed(int64_t navigation_id,
                                     bool should_replace_current_entry,
                                     blink::mojom::CommitResult result);

  // Called by the renderer process when it is done processing a cross-document
  // commit request.
  // TODO(https://crbug.com/1020175): this is only called with
  // blink::mojom::CommitResult::Aborted.
  void OnCrossDocumentCommitProcessed(NavigationRequest* navigation_request,
                                      blink::mojom::CommitResult result);

  // Creates a TracedValue object containing the details of a committed
  // navigation, so it can be logged with the tracing system.
  std::unique_ptr<base::trace_event::TracedValue> CommitAsTracedValue(
      FrameHostMsg_DidCommitProvisionalLoad_Params* params) const;

  // Creates URLLoaderFactory objects for |isolated_world_origins|.
  //
  // Properties of the factories (e.g. their client security state) are either
  // based on the |navigation_request|, or (if |navigation_request| is null) on
  // the last committed navigation.
  blink::PendingURLLoaderFactoryBundle::OriginMap
  CreateURLLoaderFactoriesForIsolatedWorlds(
      NavigationRequest* navigation_request,
      const base::flat_set<url::Origin>& isolated_world_origins);

  // Based on the termination |status| and |exit_code|, may generate a crash
  // report to be routed to the Reporting API.
  void MaybeGenerateCrashReport(base::TerminationStatus status, int exit_code);

  // Move every child frame into the pending deletion state.
  // For each process, send the command to delete the local subtree and execute
  // the unload handlers.
  void StartPendingDeletionOnSubtree();

  // This function checks whether a pending deletion frame and all of its
  // subframes have completed running unload handlers. If so, this function
  // destroys this frame. This will happen as soon as...
  // 1) The children in other processes have been deleted.
  // 2) The ack (FrameHostMsg_Unload_ACK or mojom::FrameHost::Detach) has been
  //    received. It means this frame in the renderer process is gone.
  void PendingDeletionCheckCompleted();

  // Call |PendingDeletionCheckCompleted| recursively on this frame and its
  // children. This is useful for pruning frames with no unload handlers from
  // this frame's subtree.
  void PendingDeletionCheckCompletedOnSubtree();

  // In this frame and its children, removes every:
  // - NavigationRequest.
  // - Speculative RenderFrameHost.
  void ResetNavigationsForPendingDeletion();

  // Called on an unloading frame when its unload timeout is reached. This
  // immediately deletes the RenderFrameHost.
  void OnUnloadTimeout();

  // Runs interception set up in testing code, if any.
  // Returns true if we should proceed to the Commit callback, false otherwise.
  bool MaybeInterceptCommitCallback(
      NavigationRequest* navigation_request,
      FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params);

  // If this RenderFrameHost is a local root (i.e., either the main frame or a
  // subframe in a different process than its parent), this returns the
  // RenderWidgetHost corresponding to this frame. Otherwise this returns null.
  // See also GetRenderWidgetHost(), which walks up the tree to find the nearest
  // local root.
  // Main frame: RenderWidgetHost is owned by the RenderViewHost.
  // Subframe: RenderWidgetHost is owned by this RenderFrameHost.
  RenderWidgetHostImpl* GetLocalRenderWidgetHost() const;

  // Called after a new document commit. Every children of the previous document
  // are expected to be deleted or at least to be pending deletion waiting for
  // unload completion. A compromised renderer process or bugs can cause the
  // renderer to "forget" to start deletion. In this case the browser deletes
  // them immediately, without waiting for unload completion.
  // https://crbug.com/950625.
  void EnsureDescendantsAreUnloading();

  // Implements AddMessageToConsole() and AddUniqueMessageToConsole().
  void AddMessageToConsoleImpl(blink::mojom::ConsoleMessageLevel level,
                               const std::string& message,
                               bool discard_duplicates);

  // Helper functions for logging crash keys when ValidateDidCommitParams()
  // determines it cannot commit a URL or origin.
  void LogCannotCommitUrlCrashKeys(const GURL& url,
                                   bool is_same_document_navigation,
                                   NavigationRequest* navigation_request);
  void LogCannotCommitOriginCrashKeys(bool is_same_document_navigation,
                                      NavigationRequest* navigation_request);

  // Evicts the document from the BackForwardCache if it is in the cache,
  // and ineligible for caching.
  void MaybeEvictFromBackForwardCache();

  // Common handler for displaying a javascript dialog from the Run*Dialog
  // mojo handlers. This method sets up some initial state before asking the
  // delegate to create a dialog.
  void RunJavaScriptDialog(const base::string16& message,
                           const base::string16& default_prompt,
                           JavaScriptDialogType dialog_type,
                           JavaScriptDialogCallback callback);

  // Callback function used to handle the dialog being closed. It will reset
  // the state in the associated RenderFrameHostImpl and call the associated
  // callback when done.
  void JavaScriptDialogClosed(JavaScriptDialogCallback response_callback,
                              bool success,
                              const base::string16& user_input);

  // See |SetIsXrOverlaySetup()|
  bool HasSeenRecentXrOverlaySetup();

  bool has_unload_handlers() {
    return has_unload_handler_ || has_pagehide_handler_ ||
           has_visibilitychange_handler_ || do_not_delete_for_testing_;
  }

  // Updates the |lifecycle_state_|. Called when there is a change in the
  // RenderFrameHost LifecycleState.
  void SetLifecycleState(LifecycleState state);

  void BindReportingObserver(
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          reporting_observer_receiver);

  // Check the renderer provided sandbox flags matches with what the browser
  // process computed on its own. This triggers DCHECK and DumpWithoutCrashing()
  //
  // TODO(https://crbug.com/1041376) Remove this when we are confident the value
  // computed from the browser is always matching.
  void CheckSandboxFlags();

  // Sets the embedding token corresponding to the document in this
  // RenderFrameHost.
  void SetEmbeddingToken(const base::UnguessableToken& embedding_token);

  // Records a DocumentCreated UKM event. Called when a Document is committed in
  // this frame.
  void RecordDocumentCreatedUkmEvent(const url::Origin& origin,
                                     const ukm::SourceId document_ukm_source_id,
                                     ukm::UkmRecorder* ukm_recorder);

  // The RenderViewHost that this RenderFrameHost is associated with.
  //
  // It is kept alive as long as any RenderFrameHosts or RenderFrameProxyHosts
  // are using it.
  //
  // The refcount allows each RenderFrameHostManager to just care about
  // RenderFrameHosts, while ensuring we have a RenderViewHost for each
  // RenderFrameHost.
  //
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  scoped_refptr<RenderViewHostImpl> render_view_host_;

  RenderFrameHostDelegate* const delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  const scoped_refptr<SiteInstanceImpl> site_instance_;

  // The agent scheduling group this RenderFrameHost is associated with. It is
  // initialized through a call to site_instance_->GetAgentSchedulingGroupHost()
  // at creation time. This cached pointer is used to avoid recreating the
  // renderer process if it has crashed, since using
  // SiteInstance::GetProcess()/GetAgentSchedulingGroupHost() has the side
  // effect of creating the process again if it is gone.
  AgentSchedulingGroupHost& agent_scheduling_group_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  FrameTree* const frame_tree_;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  FrameTreeNode* const frame_tree_node_;

  // The immediate children of this specific frame.
  std::vector<std::unique_ptr<FrameTreeNode>> children_;

  // The active parent RenderFrameHost for this frame, if it is a subframe.
  // Null for the main frame.  This is cached because the parent FrameTreeNode
  // may change its current RenderFrameHost while this child is pending
  // deletion, and GetParent() should never return a different value, even if
  // this RenderFrameHost is on the pending deletion list and the parent
  // FrameTreeNode has changed its current RenderFrameHost.
  RenderFrameHostImpl* const parent_;

  // Track this frame's last committed URL.
  GURL last_committed_url_;

  // Track the frame priority of the last committed document, which is nullopt
  // prior to the first commit.
  base::Optional<RenderProcessHostImpl::FramePriority>
      last_committed_document_priority_;

  // Track this frame's last committed origin.
  url::Origin last_committed_origin_;

  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy_;

  network::CrossOriginOpenerPolicy cross_origin_opener_policy_;

  // Track the SiteInfo of the last site we committed successfully, as obtained
  // from SiteInstanceImpl::GetSiteInfoForURL().
  SiteInfo last_committed_site_info_;

  // The most recent non-error URL to commit in this frame.
  // TODO(clamy): Remove this in favor of GetLastCommittedURL().
  // See https://crbug.com/588314.
  GURL last_successful_url_;

  // The http method of the last committed navigation.
  std::string last_http_method_;

  // The http status code of the last committed navigation.
  int last_http_status_code_ = 0;

  // Local root subframes directly own their RenderWidgetHost.
  // Please see comments about the GetLocalRenderWidgetHost() function.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  std::unique_ptr<RenderWidgetHostImpl> owned_render_widget_host_;

  const int routing_id_;

  // Boolean indicating whether this RenderFrameHost is being actively used or
  // is waiting for FrameHostMsg_Unload_ACK and thus pending deletion.
  bool is_waiting_for_unload_ack_;

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.
  bool render_frame_created_;

  // Tracks whether the RenderFrame has ever been created for this
  // RenderFrameHost or not. This starts out as false, becomes true after the
  // first call to SetRenderFrameCreated(true), and stays true thereafter.
  bool was_render_frame_ever_created_ = false;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Set to true when there is a pending FrameMsg_BeforeUnload message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_completion_;

  // Valid only when |is_waiting_for_beforeunload_completion_| is true. This
  // indicates whether a subsequent request to launch a modal dialog should be
  // honored or whether it should implicitly cause the unload to be canceled.
  bool beforeunload_dialog_request_cancels_unload_;

  // Valid only when is_waiting_for_beforeunload_completion_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true).
  bool unload_ack_is_for_navigation_;

  // The timeout monitor that runs from when the beforeunload is started in
  // DispatchBeforeUnload() until either the render process invokes the
  // respective completion callback (ProcessBeforeUnloadCompleted()), or until
  // the timeout triggers.
  std::unique_ptr<TimeoutMonitor> beforeunload_timeout_;

  // The delay to use for the beforeunload timeout monitor above.
  base::TimeDelta beforeunload_timeout_delay_;

  // When this frame is asked to execute beforeunload, this maintains a list of
  // frames that need beforeunload completion callbacks to be invoked on.  This
  // may include this frame and/or its descendant frames.  This excludes frames
  // that don't have beforeunload handlers defined.
  //
  // TODO(alexmos): For now, this always includes the navigating frame.  Make
  // this include the navigating frame only if it has a beforeunload handler
  // defined.
  std::set<RenderFrameHostImpl*> beforeunload_pending_replies_;

  // During beforeunload, keeps track whether a dialog has already been shown.
  // Used to enforce at most one dialog per navigation.  This is tracked on the
  // frame that is being navigated, and not on any of its subframes that might
  // have triggered a dialog.
  bool has_shown_beforeunload_dialog_ = false;

  // Returns whether the tab was previously discarded.
  // This is passed to CommitNavigationParams in NavigationRequest.
  bool was_discarded_;

  // Indicates whether this RenderFrameHost is in the process of loading a
  // document or not.
  bool is_loading_;

  // Indicates whether this RenderFrameHost has completed firing
  // DOMContentLoaded or not.
  bool dom_content_loaded_ = false;

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.  Tracking this
  // allows us to send things like title and state updates to the latest
  // relevant NavigationEntry.
  int nav_entry_id_;

  // Used to clean up this RFH when the unload event is taking too long to
  // execute. May be null in tests.
  std::unique_ptr<TimeoutMonitor> unload_event_monitor_timeout_;

  // GeolocationService which provides Geolocation.
  std::unique_ptr<GeolocationServiceImpl> geolocation_service_;

  // IdleManager which provides Idle status.
  std::unique_ptr<IdleManagerImpl> idle_manager_;

  // SensorProvider proxy which acts as a gatekeeper to the real SensorProvider.
  std::unique_ptr<SensorProviderProxyImpl> sensor_provider_proxy_;

  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_registry_;

  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces_;

  std::list<std::unique_ptr<WebBluetoothServiceImpl>> web_bluetooth_services_;

  std::unique_ptr<ScreenEnumerationImpl> screen_enumeration_impl_;

  // The object managing the accessibility tree for this frame.
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // This is nonzero if we sent an accessibility reset to the renderer and
  // we're waiting for an IPC containing this reset token (sequentially
  // assigned) and a complete replacement accessibility tree.
  int accessibility_reset_token_;

  // A count of the number of times we needed to reset accessibility, so
  // we don't keep trying to reset forever.
  int accessibility_reset_count_;

  // The last AXTreeData for this frame received from the RenderFrame.
  ui::AXTreeData ax_tree_data_;

  // The AX tree ID of the embedder, if this is a browser plugin guest.
  ui::AXTreeID browser_plugin_embedder_ax_tree_id_;

  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
#if defined(OS_ANDROID)
  base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>
      smart_clip_callbacks_;
#endif  // defined(OS_ANDROID)

  // Callback when an event is received, for testing.
  AccessibilityCallbackForTesting accessibility_testing_callback_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_;

  // Context shared for each mojom::PermissionService instance created for this
  // RFH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // Holder of Mojo connection with ImageDownloader service in Blink.
  mojo::Remote<blink::mojom::ImageDownloader> mojo_image_downloader_;

  // Holder of Mojo connection with FindInPage service in Blink.
  mojo::AssociatedRemote<blink::mojom::FindInPage> find_in_page_;

  // Holder of Mojo connection with the LocalFrame in Blink.
  mojo::AssociatedRemote<blink::mojom::LocalFrame> local_frame_;

  // Holder of Mojo connection with the LocalMainFrame in Blink. This
  // remote will be valid when the frame is the active main frame.
  mojo::AssociatedRemote<blink::mojom::LocalMainFrame> local_main_frame_;

  // Holder of Mojo connection with the HighPriorityLocalFrame in blink.
  mojo::Remote<blink::mojom::HighPriorityLocalFrame> high_priority_local_frame_;

  // Holds a NavigationRequest when it's about to commit, ie. after
  // OnCrossDocumentCommitProcessed has returned a positive answer for this
  // NavigationRequest but before receiving DidCommitProvisionalLoad. This
  // NavigationRequest is for a cross-document navigation.
  std::unique_ptr<NavigationRequest> navigation_request_;

  // Holds AppCacheNavigationHandle after navigation request has been committed,
  // which keeps corresponding AppCacheHost alive while renderer asks for it.
  // See AppCacheNavigationHandle comment for more details.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  // Holds the cross-document NavigationRequests that are waiting to commit,
  // indexed by IDs. These are navigations that have passed ReadyToCommit stage
  // and are waiting for the renderer to send back a matching
  // OnCrossDocumentCommitProcessed.

  // TODO(ahemery): We have this storage as a map because we actually want to
  // find navigations by id with PerNavigationMojoInterface disabled.
  // When the flag is always on, rework the structure to simply store an
  // unindexed bunch of ongoing navigations and modify
  // DidCommitNavigationInternal.
  std::map<NavigationRequest*, std::unique_ptr<NavigationRequest>>
      navigation_requests_;

  // Holds a same-document NavigationRequest while waiting for the navigation it
  // is tracking to commit.
  std::unique_ptr<NavigationRequest> same_document_navigation_request_;

  // The associated WebUIImpl and its type. They will be set if the current
  // document is from WebUI source. Otherwise they will be null and
  // WebUI::kNoWebUI, respectively.
  std::unique_ptr<WebUIImpl> web_ui_;
  WebUI::TypeID web_ui_type_;

  // If true, then the RenderFrame has selected text.
  bool has_selection_;

  // If true, then this RenderFrame has one or more audio streams with audible
  // signal. If false, all audio streams are currently silent (or there are no
  // audio streams).
  bool is_audible_;

  // If true, then the Virtual keyboard rectangle that occludes the content is
  // sent to the VirtualKeyboard API where it fires overlaygeometrychange JS
  // event notifying the web authors that Virtual keyboard has occluded the
  // content.
  bool should_virtual_keyboard_overlay_content_;

  // Used for tracking the latest size of the RenderFrame.
  base::Optional<gfx::Size> frame_size_;

  // The Previews state of the last navigation. This is used during history
  // navigation of subframes to ensure that subframes navigate with the same
  // Previews status as the top-level frame.
  blink::PreviewsState last_navigation_previews_state_;

  // This boolean indicates whether the RenderFrame has committed *any*
  // navigation or not. Starts off false and is set to true for the lifetime of
  // the RenderFrame when the first CommitNavigation message is sent to the
  // RenderFrame. It is reset after a renderer process crash.
  bool has_committed_any_navigation_ = false;
  bool must_be_replaced_ = false;

  mojo::AssociatedReceiver<mojom::FrameHost> frame_host_associated_receiver_{
      this};
  mojo::AssociatedReceiver<blink::mojom::BackForwardCacheControllerHost>
      back_forward_cache_controller_host_associated_receiver_{this};
  mojo::Remote<mojom::Frame> frame_;
  mojo::AssociatedRemote<mojom::FrameBindingsControl> frame_bindings_control_;
  mojo::AssociatedRemote<mojom::FrameNavigationControl> navigation_control_;
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>
      local_frame_host_receiver_{this};

  // This receiver should only be valid when the frame is a swapped in main
  // frame.
  mojo::AssociatedReceiver<blink::mojom::LocalMainFrameHost>
      local_main_frame_host_receiver_{this};

  // If this is true then this object was created in response to a renderer
  // initiated request. Init() will be called, and until then navigation
  // requests should be queued.
  bool waiting_for_init_;

  // If true then this frame's document has a focused element which is editable.
  bool has_focused_editable_element_;

  std::unique_ptr<PendingNavigation> pending_navigate_;

  // A collection of non-network URLLoaderFactory implementations which are used
  // to service any supported non-network subresource requests for the currently
  // committed navigation.
  ContentBrowserClient::NonNetworkURLLoaderFactoryDeprecatedMap
      non_network_uniquely_owned_factories_;

  // Renderer-side states that blocks fast shutdown of the frame.
  bool has_before_unload_handler_ = false;
  bool has_unload_handler_ = false;
  bool has_pagehide_handler_ = false;
  bool has_visibilitychange_handler_ = false;

  base::Optional<RenderFrameAudioOutputStreamFactory>
      audio_service_audio_output_stream_factory_;
  base::Optional<RenderFrameAudioInputStreamFactory>
      audio_service_audio_input_stream_factory_;

  // Hosts media::mojom::InterfaceFactory for the RenderFrame and forwards
  // media::mojom::InterfaceFactory calls to the remote "media" service.
  std::unique_ptr<MediaInterfaceProxy> media_interface_proxy_;

  // Hosts blink::mojom::PresentationService for the RenderFrame.
  std::unique_ptr<PresentationServiceImpl> presentation_service_;

  // Hosts blink::mojom::FileSystemManager for the RenderFrame.
  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_;

  // Hosts blink::mojom::PushMessaging for the RenderFrame.
  std::unique_ptr<PushMessagingManager, base::OnTaskRunnerDeleter>
      push_messaging_manager_;

  // Hosts blink::mojom::SpeechSynthesis for the RenderFrame.
  std::unique_ptr<SpeechSynthesisImpl> speech_synthesis_impl_;

#if !defined(OS_ANDROID)
  std::unique_ptr<AuthenticatorImpl> authenticator_impl_;
#endif

  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // A bitwise OR of bindings types that have been enabled for this RenderFrame.
  // See BindingsPolicy for details.
  int enabled_bindings_ = 0;

  // Tracks the feature policy which has been set on this frame.
  std::unique_ptr<blink::FeaturePolicy> feature_policy_;

  // Tracks the sandbox flags which are in effect on this frame. This includes
  // any flags which have been set by a Content-Security-Policy header, in
  // addition to those which are set by the embedding frame. This is initially a
  // copy of the active sandbox flags which are stored in the FrameTreeNode for
  // this RenderFrameHost, but may diverge if this RenderFrameHost is pending
  // deletion.
  network::mojom::WebSandboxFlags active_sandbox_flags_;

  // Same as |active_sandbox_flags_|, except this is computed:
  // - outside of the renderer process.
  // - before loading the document.
  //
  // For now, this is simply used to double check this matches the renderer
  // computation. Later this will be used as the source of truth.
  //
  // [OutOfBlinkSandbox](https://crbug.com/1041376)
  base::Optional<network::mojom::WebSandboxFlags> active_sandbox_flags_control_;

  // Tracks the document policy which has been set on this frame.
  std::unique_ptr<blink::DocumentPolicy> document_policy_;

#if defined(OS_ANDROID)
  // An InterfaceProvider for Java-implemented interfaces that are scoped to
  // this RenderFrameHost. This provides access to interfaces implemented in
  // Java in the browser process to C++ code in the browser process.
  std::unique_ptr<service_manager::InterfaceProvider> java_interfaces_;

  // An InterfaceRegistry that forwards interface requests from Java to the
  // RenderFrame. This provides access to interfaces implemented in the renderer
  // to Java code in the browser process.
  class JavaInterfaceProvider;
  std::unique_ptr<JavaInterfaceProvider> java_interface_registry_;
#endif

  // Binding for the InterfaceProvider through which this RenderFrameHostImpl
  // exposes frame-scoped Mojo services to the currently active document in the
  // corresponding RenderFrame.
  //
  // GetInterface messages dispatched through this binding are guaranteed to
  // originate from the document corresponding to the last committed navigation;
  // or the initial empty document if no real navigation has ever been
  // committed.
  //
  // The InterfaceProvider interface connection is established as follows:
  //
  // 1) For the initial empty document, the call site that creates this
  //    RenderFrameHost is responsible for creating a message pipe, binding its
  //    receiver end to this instance by calling
  //    BindInterfaceProviderReceiver(), and plumbing the client end to the
  //    renderer process, and ultimately supplying it to the RenderFrame
  //    synchronously at construction time.
  //
  //    The only exception to this rule are out-of-process child frames, whose
  //    RenderFrameHosts take care of this internally in CreateRenderFrame().
  //
  // 2) For subsequent documents, the RenderFrame creates a new message pipe
  //    every time a cross-document navigation is committed, and pushes its
  //    request end to the browser process as part of DidCommitProvisionalLoad.
  //    The client end will be used by the new document corresponding to the
  //    committed naviagation to access services exposed by the RenderFrameHost.
  //
  // This is required to prevent GetInterface messages racing with navigation
  // commit from being serviced in the security context corresponding to the
  // wrong document in the RenderFrame. The benefit of the approach taken is
  // that it does not necessitate using channel-associated InterfaceProvider
  // interfaces.
  mojo::Receiver<service_manager::mojom::InterfaceProvider>
      document_scoped_interface_provider_receiver_{this};

  // BrowserInterfaceBroker implementation through which this
  // RenderFrameHostImpl exposes document-scoped Mojo services to the currently
  // active document in the corresponding RenderFrame.
  BrowserInterfaceBrokerImpl<RenderFrameHostImpl, RenderFrameHost*> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // Logs interface requests that arrive after the frame has already committed a
  // non-same-document navigation, and has already unbound
  // |broker_receiver_| from the interface connection that had been used to
  // service RenderFrame::GetBrowserInterfaceBroker for the previously active
  // document in the frame.
  std::unique_ptr<DroppedInterfaceRequestLogger>
      dropped_interface_request_logger_;

  // IPC-friendly token that represents this host.
  const base::UnguessableToken frame_token_;

  // Binding to remote implementation of mojom::RenderAccessibility. Note that
  // this binding is done on-demand (in UpdateAccessibilityMode()) and will only
  // be connected (i.e. bound) to the other endpoint in the renderer while there
  // is an accessibility mode that includes |kWebContents|.
  mojo::AssociatedRemote<mojom::RenderAccessibility> render_accessibility_;

  mojo::AssociatedReceiver<mojom::RenderAccessibilityHost>
      render_accessibility_host_receiver_{this};

  mojo::AssociatedReceiver<mojom::DomAutomationControllerHost>
      dom_automation_controller_receiver_{this};

  std::unique_ptr<KeepAliveHandleFactory> keep_alive_handle_factory_;
  base::TimeDelta keep_alive_timeout_;

  // For observing Network Service connection errors only. Will trigger
  // |OnNetworkServiceConnectionError()| and push updated factories to
  // |RenderFrame|.
  mojo::Remote<network::mojom::URLLoaderFactory>
      network_service_disconnect_handler_holder_;

  // Whether UpdateSubresourceLoaderFactories should recreate the default
  // URLLoaderFactory when handling a NetworkService crash.  In case the frame
  // is covered by AppCache, only isolated-world-specific factories need to be
  // refreshed, but the main, AppCache-specific factory shouldn't be refreshed.
  bool recreate_default_url_loader_factory_after_network_service_crash_ = false;

  // Set of isolated world origins that require a separate URLLoaderFactory
  // (e.g. for handling requests initiated by extension content scripts that
  // require relaxed CORS/CORB rules).
  base::flat_set<url::Origin>
      isolated_worlds_requiring_separate_url_loader_factory_;

  // Holds the renderer generated ID and global request ID for the main frame
  // request.
  std::pair<int, GlobalRequestID> main_frame_request_ids_;

  // If |ResourceLoadComplete()| is called for the main resource before
  // |DidCommitProvisionalLoad()|, the load info is saved here to call
  // |ResourceLoadComplete()| when |DidCommitProvisionalLoad()| is called. This
  // is necessary so the renderer ID can be mapped to the global ID in
  // |DidCommitProvisionalLoad()|. This situation should only happen when an
  // empty document is loaded.
  blink::mojom::ResourceLoadInfoPtr deferred_main_frame_load_info_;

  // If a subframe failed to finish running its unload handler after
  // |subframe_unload_timeout_| the RenderFrameHost is deleted.
  base::TimeDelta subframe_unload_timeout_;

  // Call OnUnloadTimeout() when the unload timer expires.
  base::OneShotTimer subframe_unload_timer_;

  // BackForwardCache:
  bool is_evicted_from_back_forward_cache_ = false;
  base::OneShotTimer back_forward_cache_eviction_timer_;

  // The reasons given in BackForwardCache::DisableForRenderFrameHost. This is a
  // breakdown of NotRestoredReason::kDisableForRenderFrameHostCalled.
  std::set<std::string> back_forward_cache_disabled_reasons_;

  // Whether proactive BrowsingInstance swap is disabled for this frame or not.
  // Note that even if this is false, proactive BrowsingInstance swap still
  // might not happen on navigations on this frame due to other reasons.
  // Should only be used for testing purposes.
  bool has_test_disabled_proactive_browsing_instance_swap_ = false;

  // This used to re-commit when restoring from the BackForwardCache, with the
  // same params as the original navigation.
  // Note: If BackForwardCache is not enabled, this field is not set.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
      last_commit_params_;

  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;

  // Whether the currently committed document is MHTML or not. It is set at
  // commit time based on the MIME type of the NavigationRequest that resulted
  // in the navigation commit. Setting the value should be based only on
  // browser side state as this value is used in security checks.
  bool is_mhtml_document_ = false;

  // The last reported character encoding, not canonicalized.
  std::string last_reported_encoding_;

  // The canonicalized character encoding.
  std::string canonical_encoding_;

  // Used to intercept DidCommit* calls in tests.
  CommitCallbackInterceptor* commit_callback_interceptor_;

  // Mask of the active features tracked by the scheduler used by this frame.
  // This is used only for metrics.
  // See blink::SchedulingPolicy::Feature for the meaning.
  // These values should be cleared on document commit.
  // Both are needed as some features are tracked in the renderer process and
  // some in the browser process, depending on the design of each individual
  // feature. They are tracked separately, because when the renderer updates the
  // set of features, the browser ones should persist.
  uint64_t renderer_reported_scheduler_tracked_features_ = 0;
  uint64_t browser_reported_scheduler_tracked_features_ = 0;

  // Holds prefetched signed exchanges for SignedExchangeSubresourcePrefetch.
  // They will be passed to the next navigation.
  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Isolation information to be used for subresources from the currently
  // committed navigation. Stores both the SiteForCookies and the
  // NetworkIsolationKey. This is specific to a document and should be reset on
  // every cross-document commit. When a new frame is created, the new frame
  // inherits the IsolationInfo from the creator frame, similarly to the last
  // committed origin.
  net::IsolationInfo isolation_info_;

  // The factory to load resources from the WebBundle source bound to
  // this file.
  std::unique_ptr<WebBundleHandle> web_bundle_handle_;

  // Tainted once MediaStream access was granted.
  bool was_granted_media_access_ = false;

  // Salt for generating frame-specific media device IDs.
  std::string media_device_id_salt_base_;

  // Keeps track of various security properties of the last committed document
  // that are needed by the network service.
  network::mojom::ClientSecurityStatePtr last_committed_client_security_state_;

  // Keep the list of ServiceWorkerContainerHosts so that they can observe when
  // the frame goes in/out of BackForwardCache.
  // These pointers must be dereferenced on the
  // |ServiceWorkerContext::GetCoreThreadId()| thread only.
  // TODO(yuzus): Make this a single pointer. A frame should only have a single
  // container host, but probably during a navigation the old container host is
  // still alive when the new container host is created and added to this
  // vector, and the old container host is destroyed shortly after navigation.
  std::map<std::string, base::WeakPtr<ServiceWorkerContainerHost>>
      service_worker_container_hosts_;
  // Keeps the track of the latest ServiceWorkerContainerHost.
  base::WeakPtr<ServiceWorkerContainerHost> last_committed_service_worker_host_;

  // The reason why the last attempted navigation in the frame didn't use a new
  // BrowsingInstance.
  // TODO(crbug.com/1026101): Remove after the investigation.
  base::Optional<ShouldSwapBrowsingInstance>
      browsing_instance_not_swapped_reason_;

  // The portals owned by this frame. |Portal::owner_render_frame_host_| points
  // back to |this|.
  base::flat_set<std::unique_ptr<Portal>, base::UniquePtrComparator> portals_;

  // Tracking active features in this frame, for use in figuring out whether
  // or not it can be frozen.
  std::unique_ptr<FeatureObserver> feature_observer_;

  // Optional PeakGpuMemoryTracker, when this frame is the main frame. Created
  // by NavigationRequest, ownership is maintained until the frame has stopped
  // loading. Or newer navigations occur.
  std::unique_ptr<PeakGpuMemoryTracker> loading_mem_tracker_ = nullptr;

  scoped_refptr<WebAuthRequestSecurityChecker>
      webauth_request_security_checker_;

  // Container for arbitrary document-associated feature-specific data. Should
  // be reset when committing a cross-document navigation in this
  // RenderFrameHost. Please refer to the description at
  // content/public/browser/render_document_host_user_data.h for more details.
  class DocumentAssociatedData : public base::SupportsUserData {
    friend class RenderFrameHostImpl;
  };
  DocumentAssociatedData document_associated_data_;

  // Keeps track of the scenario when RenderFrameHostManager::CommitPending is
  // called before the navigation commits. This becomes true if the previous
  // RenderFrameHost is not alive and the speculative RenderFrameHost is
  // committed early (see RenderFrameHostManager::GetFrameHostForNavigation for
  // more details). While |committed_speculative_rfh_before_navigation_commit_|
  // is true the RenderFrameHost which we commit early will be live.
  bool committed_speculative_rfh_before_navigation_commit_ = false;

  // This time is used to record the last WebXR DOM Overlay setup request.
  base::TimeTicks last_xr_overlay_setup_time_;

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter_;
  std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter_;

  // https://github.com/camillelamy/explainers/blob/master/coop_reporting.md#virtual-browsing-context-group-id
  //
  // Whenever we detect that the enforcement of a report-only COOP policy would
  // have resulted in a BrowsingInstance switch, we assign a new virtual
  // browsing context group ID to the RenderFrameHostImpl that has navigated.
  int virtual_browsing_context_group_;

  // Navigation ID for the last committed cross-document non-bfcached navigation
  // in this RenderFrameHost.
  // TODO(crbug.com/936696): Make this const after we have RenderDocument.
  int64_t last_committed_cross_document_navigation_id_ = -1;

  // Tracks the state of |this| RenderFrameHost from the point it is created to
  // when it gets deleted.
  LifecycleState lifecycle_state_;

  // If true, RenderFrameHost should not be actually deleted and should be left
  // stuck in pending deletion.
  bool do_not_delete_for_testing_ = false;

  // Embedding token for the document in this RenderFrameHost. This differs from
  // |frame_token_| in that |frame_token_| has a lifetime matching that of the
  // corresponding RenderFrameHostImpl, and is intended to be used for IPCs for
  // identifying frames just like routing IDs. |embedding_token_| has a document
  // scoped lifetime and changes on cross-document navigations.
  base::Optional<base::UnguessableToken> embedding_token_;

  // Observers listening to cookie access notifications for the current document
  // in this RenderFrameHost.
  // Note: at the moment this set is not cleared when a new document is created
  // in this RenderFrameHost. This is done because the first observer is created
  // before the navigation actually commits and because the old routing id-based
  // behaved in the same way as well.
  // This problem should go away with RenderDocumentHost in any case.
  // TODO(crbug.com/936696): Remove this warning after the RDH ships.
  mojo::ReceiverSet<network::mojom::CookieAccessObserver> cookie_observers_;

  // Indicates whether this frame is an outer delegate frame for some other
  // RenderFrameHost.
  bool is_outer_delegate_frame_ = false;

  // Indicates whether navigator.credentials.get({otp: {transport:"sms"}}) has
  // been used on a document (regardless of the outcome).
  // Note that WebOTP is not allowed in iframes for security reasons. i.e. this
  // will not be set in such case which is expected. In addition, since the
  // RenderFrameHost may persist across navigations, we need to reset the bit
  // to make sure that it's used per document.
  bool document_used_web_otp_ = false;

  // The browsing context's required CSP as defined by
  // https://w3c.github.io/webappsec-cspee/#required-csp,
  // stored when the frame commits the navigation.
  network::mojom::ContentSecurityPolicyPtr required_csp_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

// Used when DCHECK_STATE_TRANSITION triggers.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& o,
    const RenderFrameHostImpl::LifecycleState& s);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
