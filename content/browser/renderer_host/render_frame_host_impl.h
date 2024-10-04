// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/process/kill.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/buckets/bucket_context.h"
#include "content/browser/can_commit_status.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/browsing_context_state.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/document_associated_data.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/keep_alive_handle_factory.h"
#include "content/browser/renderer_host/loading_state.h"
#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"
#include "content/browser/renderer_host/origin_trial_state_host_impl.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/transient_allow_popup.h"
#include "content/browser/renderer_host/transient_focus_source_user_activation.h"
#include "content/browser/security/coop/cross_origin_opener_policy_access_report_manager.h"
#include "content/browser/security/coop/cross_origin_opener_policy_reporter.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/dom_automation_controller.mojom.h"
#include "content/common/features.h"
#include "content/common/frame.mojom.h"
#include "content/common/input/input_injector.mojom-forward.h"
#include "content/common/navigation_client.mojom-forward.h"
#include "content/common/navigation_client.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_discard_reason.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/extra_mojo_js_features.mojom-forward.h"
#include "content/public/common/javascript_dialog_type.h"
#include "media/mojo/mojom/interface_factory.mojom-forward.h"
#include "media/mojo/mojom/key_system_support.mojom-forward.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-forward.h"
#include "media/mojo/mojom/media_player.mojom-forward.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom-forward.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "media/mojo/services/mojo_video_encoder_metrics_provider_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/net_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom-forward.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/delegated_capability_request_token.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/history_user_activation_state.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-forward.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-forward.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom-forward.h"
#include "third_party/blink/public/mojom/origin_trial_state/origin_trial_state_host.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-forward.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider.mojom-forward.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-forward.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom-forward.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/mojom/ax_updates_and_events.mojom.h"
#include "ui/accessibility/platform/ax_node_id_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/containers/id_map.h"
#include "content/browser/webauth/webauth_request_security_checker.h"
#include "services/device/public/mojom/nfc.mojom.h"
#else
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"
#include "third_party/blink/public/mojom/serial/serial.mojom-forward.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-forward.h"
#endif

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
#include "media/mojo/mojom/remoting.mojom-forward.h"
#endif

namespace blink {
class AssociatedInterfaceRegistry;
class DocumentPolicy;
struct FramePolicy;
struct TransferableMessage;
struct UntrustworthyContextMenuParams;

namespace mojom {
class CacheStorage;
class DeviceAPIService;
class GeolocationService;
class ManagedConfigurationService;
class WebUsbService;
}  // namespace mojom
}  // namespace blink

namespace gfx {
class Range;
}

namespace input {
class TimeoutMonitor;
}  // namespace input

namespace mojo {
class MessageFilter;
class UrgentMessageScope;
}

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace ukm {
class UkmRecorder;
}

namespace features {

// Feature to prevent name updates to RenderFrameHost (and by extension its
// relevant BrowsingContextState) when it is not current (i.e. is in the
// BackForwardCache or is pending delete). This primarily will affect the
// non-legacy implementation of BrowsingContextState.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kDisableFrameNameUpdateOnNonCurrentRenderFrameHost);

// Feature to evict when accessibility events occur while in back/forward cache.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEvictOnAXEvents);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kDoNotEvictOnAXLocationChange);

}  // namespace features

namespace content {

class AgentSchedulingGroupHost;
class BrowsingContextState;
class CodeCacheHostImpl;
class CrossOriginEmbedderPolicyReporter;
class CrossOriginOpenerPolicyAccessReportManager;
class FeatureObserver;
class FencedFrame;
class FileSystemManagerImpl;
class FrameTree;
class FrameTreeNode;
class GeolocationServiceImpl;
class IdleManagerImpl;
class NavigationEarlyHintsManager;
class NavigationRequest;
class PeerConnectionTrackerHost;
class PrefetchedSignedExchangeCache;
class PrerenderCancellationReason;
class PresentationServiceImpl;
class PushMessagingManager;
class RenderAccessibilityHost;
class RenderFrameHostDelegate;
class RenderFrameHostImpl;
class RenderFrameHostImplPpapiSupport;
class RenderFrameHostManager;
class RenderFrameHostOrProxy;
class RenderFrameHostOwner;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostView;
class ServiceWorkerClient;
class SiteInfo;
class SpeechSynthesisImpl;
class WebAuthRequestSecurityChecker;
class WebUIImpl;
struct PendingNavigation;
struct ResourceTimingInfo;

// To be called when a RenderFrameHostImpl receives an event.
// Provides the host, the event fired, and which node id the event was for.
typedef base::RepeatingCallback<
    void(RenderFrameHostImpl*, ax::mojom::Event, int)>
    AccessibilityCallbackForTesting;

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      public base::SupportsUserData,
      public mojom::FrameHost,
      public mojom::DomAutomationControllerHost,
      public ui::AXPlatformTreeManagerDelegate,
      public ui::AXNodeIdDelegate,
      public SiteInstanceGroup::Observer,
      public blink::mojom::AssociatedInterfaceProvider,
      public blink::mojom::BackForwardCacheControllerHost,
      public blink::mojom::LocalFrameHost,
      public blink::mojom::NonAssociatedLocalFrameHost,
      public blink::mojom::LocalMainFrameHost,
      public ui::AXActionHandlerBase,
      public device::mojom::VibrationManagerListener,
      public network::mojom::CookieAccessObserver,
      public network::mojom::TrustTokenAccessObserver,
      public network::mojom::SharedDictionaryAccessObserver,
      public BucketContext {
 public:
  using JavaScriptDialogCallback =
      content::JavaScriptDialogManager::DialogClosedCallback;

  // Callback used with IsClipboardPasteAllowed() method.
  using IsClipboardPasteAllowedCallback =
      ContentBrowserClient::IsClipboardPasteAllowedCallback;

  // Data used with IsClipboardPasteAllowedByPolicy() method.
  using ClipboardPasteData = content::ClipboardPasteData;

  static RenderFrameHostImpl* From(RenderFrameHost* render_frame_host) {
    // It is assumed that all RenderFrameHosts are RenderFrameHostImpls.
    return static_cast<RenderFrameHostImpl*>(render_frame_host);
  }

  static RenderFrameHostImpl* FromID(GlobalRenderFrameHostId id);
  static RenderFrameHostImpl* FromID(int process_id, int routing_id);
  // Returns the `RenderFrameHostImpl` with the given `blink::LocalFrameToken`,
  // or `nullptr` if no such `RenderFrameHostImpl` exists.
  //
  // Note that though local frame tokens are globally unique, the process ID is
  // required as a second factor to reduce the risk of inadvertent info leaks
  // leading to security failures. If there is a `RenderFrameHostImpl` with a
  // matching local frame token but not a matching process ID, invokes
  // `process_mismatch_callback` (if non-null) and returns `nullptr`.
  static RenderFrameHostImpl* FromFrameToken(
      const GlobalRenderFrameHostToken& frame_token,
      mojo::ReportBadMessageCallback* process_mismatch_callback = nullptr);
  static RenderFrameHostImpl* FromFrameToken(
      int process_id,
      const blink::LocalFrameToken& frame_token,
      mojo::ReportBadMessageCallback* process_mismatch_callback = nullptr);
  // Returns the `RenderFrameHostImpl` with the given `blink::DocumentToken`, or
  // `nullptr` if no such `RenderFrameHostImpl` exists.
  //
  // Note that though document tokens are globally unique, the process ID is
  // required as a second factor to reduce the risk of inadvertent info leaks
  // leading to security failures. If there is a `RenderFrameHostImpl` with a
  // matching document token but not a matching process ID, invokes
  // `process_mismatch_callback` (if non-null) and returns `nullptr`.
  static RenderFrameHostImpl* FromDocumentToken(
      int process_id,
      const blink::DocumentToken& document_token,
      mojo::ReportBadMessageCallback* process_mismatch_callback = nullptr);

  static RenderFrameHostImpl* FromAXTreeID(ui::AXTreeID ax_tree_id);
  static RenderFrameHostImpl* FromOverlayRoutingToken(
      const base::UnguessableToken& token);

  // Clears the all prefetched cached signed exchanges.
  static void ClearAllPrefetchedSignedExchangeCache();

  // Walk through all RenderFrameHosts in the specified `browser_context` and
  // cancel navigations that are happening in them. This is assumed to be called
  // on a `browser_context` that's shutting down.
  //
  // Ideally, this shouldn't be necessary, as BrowserContext cleanup should also
  // trigger features above //content to clean up their WebContents, and
  // destroying a WebContents cancels any ongoing navigations as part of
  // destroying the frame tree. Unfortunately, there are bugs where this isn't
  // happening and where the culprits are difficult to find (e.g.,
  // https://crbug.com/40062641, https://crbug.com/40243371,
  // https://crbug.com/358487779). This code attempts to limit potential damage
  // when this happens by at least making sure that any ongoing navigations are
  // canceled, since allowing them to proceed may lead to broken assumptions in
  // the navigation stack. For example, the navigation response might attempt to
  // create a RenderFrameHost in a new process in the destroyed BrowserContext
  // and crash.
  static void CancelAllNavigationsForBrowserContextShutdown(
      BrowserContext* browser_context);

  // TODO(crbug.com/40183788): Get/SetCodeCacheHostReceiverHandler are used only
  // for a test in content/browser/service_worker/service_worker_browsertest
  // that tests a bad message is returned on an incorrect origin. Try to find a
  // way to test this without adding these additional methods.
  // Allows external code to supply a callback that is invoked immediately
  // after the CodeCacheHostImpl is created and bound.  Used for swapping
  // the binding for a test version of the service.
  using CodeCacheHostReceiverHandler = base::RepeatingCallback<void(
      CodeCacheHostImpl*,
      mojo::ReceiverId,
      mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>&)>;
  static void SetCodeCacheHostReceiverHandlerForTesting(
      CodeCacheHostReceiverHandler handler);

  // Get the PolicyContainerHost associated with `frame_token`.
  static PolicyContainerHost* GetPolicyContainerHost(
      const blink::LocalFrameToken* frame_token,
      int initiator_process_id,
      StoragePartitionImpl* storage_partition);

  // Get the SiteInstance for the RenderFrameHost associated with `frame_token`,
  // looking it up via NavigationStateKeepAlive in the case that the
  // RenderFrameHost has already been deleted after initiating a scheduled
  // navigation. The `storage_partition` parameter is used for looking up
  // NavigationStateKeepAlives when needed.
  static SiteInstanceImpl* GetSourceSiteInstanceFromFrameToken(
      const blink::LocalFrameToken* frame_token,
      int initiator_process_id,
      StoragePartitionImpl* storage_partition);

  RenderFrameHostImpl(const RenderFrameHostImpl&) = delete;
  RenderFrameHostImpl& operator=(const RenderFrameHostImpl&) = delete;

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  const blink::StorageKey& GetStorageKey() const override;
  int GetRoutingID() const override;
  const blink::LocalFrameToken& GetFrameToken() const override;
  const base::UnguessableToken& GetReportingSource() override;

  ui::AXTreeID GetAXTreeID() override;
  SiteInstanceImpl* GetSiteInstance() const override;
  RenderProcessHost* GetProcess() const override;
  GlobalRenderFrameHostId GetGlobalId() const override;
  GlobalRenderFrameHostToken GetGlobalFrameToken() const override;
  RenderWidgetHostImpl* GetRenderWidgetHost() override;
  RenderWidgetHostViewBase* GetView() override;
  RenderFrameHostImpl* GetParent() const override;
  RenderFrameHostImpl* GetParentOrOuterDocument() const final;
  RenderFrameHostImpl* GetParentOrOuterDocumentOrEmbedder() const override;
  RenderFrameHostImpl* GetMainFrame() override;
  PageImpl& GetPage() override;
  bool IsInPrimaryMainFrame() override;
  RenderFrameHostImpl* GetOutermostMainFrame() override;
  RenderFrameHostImpl* GetOutermostMainFrameOrEmbedder() override;
  bool IsFencedFrameRoot() const override;
  bool IsNestedWithinFencedFrame() const override;
  void ForEachRenderFrameHostWithAction(
      base::FunctionRef<FrameIterationAction(RenderFrameHost*)> on_frame)
      override;
  void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHost*)> on_frame) override;
  // TODO (crbug.com/1251545) : Frame tree node id should only be known for
  // subframes. As such, update this method.
  FrameTreeNodeId GetFrameTreeNodeId() const override;
  const base::UnguessableToken& GetDevToolsFrameToken() override;
  std::optional<base::UnguessableToken> GetEmbeddingToken() override;
  const std::string& GetFrameName() override;
  bool IsFrameDisplayNone() override;
  const std::optional<gfx::Size>& GetFrameSize() override;
  size_t GetFrameDepth() override;
  bool IsCrossProcessSubframe() override;
  WebExposedIsolationLevel GetWebExposedIsolationLevel() override;
  const GURL& GetLastCommittedURL() const override;
  const url::Origin& GetLastCommittedOrigin() const override;
  const net::NetworkIsolationKey& GetNetworkIsolationKey() override;
  const net::IsolationInfo& GetIsolationInfoForSubresources() override;
  net::IsolationInfo GetPendingIsolationInfoForSubresources() override;
  gfx::NativeView GetNativeView() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  void ExecuteJavaScriptMethod(const std::u16string& object_name,
                               const std::u16string& method_name,
                               base::Value::List arguments,
                               JavaScriptResultCallback callback) override;
  void ExecuteJavaScript(const std::u16string& javascript,
                         JavaScriptResultCallback callback) override;
  void ExecuteJavaScriptInIsolatedWorld(const std::u16string& javascript,
                                        JavaScriptResultCallback callback,
                                        int32_t world_id) override;
  void ExecuteJavaScriptForTests(const std::u16string& javascript,
                                 JavaScriptResultCallback callback,
                                 int32_t world_id) override;
  void ExecuteJavaScriptWithUserGestureForTests(
      const std::u16string& javascript,
      JavaScriptResultCallback callback,
      int32_t world_id) override;
  void ExecutePluginActionAtLocalLocation(
      const gfx::Point& location,
      blink::mojom::PluginActionType plugin_action) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  void InsertVisualStateCallback(VisualStateCallback callback) override;
  void CopyImageAt(int x, int y) override;
  void SaveImageAt(int x, int y) override;
  RenderViewHost* GetRenderViewHost() const override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  content::PageVisibilityState GetVisibilityState() override;
  bool IsLastCommitIPAddressPubliclyRoutable() const override;
  bool IsRenderFrameLive() override;
  LifecycleState GetLifecycleState() override;
  bool IsInLifecycleState(LifecycleState lifecycle_state) override;
  bool IsActive() const override;
  bool IsInactiveAndDisallowActivation(uint64_t reason) override;
  size_t GetProxyCount() override;
  bool HasSelection() override;
  const network::mojom::URLResponseHead* GetLastResponseHead() override;
  void RequestTextSurroundingSelection(
      blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback,
      int max_length) override;
  void SendInterventionReport(const std::string& id,
                              const std::string& message) override;
  WebUI* GetWebUI() override;
  void AllowBindings(BindingsPolicySet bindings) override;
  BindingsPolicySet GetEnabledBindings() override;
  void SetWebUIProperty(const std::string& name,
                        const std::string& value) override;
  void DisableBeforeUnloadHangMonitorForTesting() override;
  bool IsBeforeUnloadHangMonitorDisabledForTesting() override;
  bool GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType disabler_type) override;
  bool IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature feature) override;
  const blink::PermissionsPolicy* GetPermissionsPolicy() override;
  const blink::ParsedPermissionsPolicy& GetPermissionsPolicyHeader() override;
  void ViewSource() override;
  void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point&,
      const blink::mojom::MediaPlayerAction& action) override;
  void RequestVideoFrameAtWithBoundsHint(
      const gfx::Point& viewport_position,
      const gfx::Size& max_size,
      int max_area,
      base::OnceCallback<void(const SkBitmap&, const gfx::Rect&)> callback)
      override;
  bool CreateNetworkServiceDefaultFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver) override;
  void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      const base::flat_set<url::Origin>& isolated_world_origins,
      bool push_to_renderer_now) override;
  bool IsSandboxed(network::mojom::WebSandboxFlags flags) override;
  void FlushNetworkAndNavigationInterfacesForTesting(
      bool do_nothing_if_no_network_service_connection) override;
  void PrepareForInnerWebContentsAttach(
      PrepareForInnerWebContentsAttachCallback callback) override;
  blink::FrameOwnerElementType GetFrameOwnerElementType() override;
  bool HasTransientUserActivation() override;
  bool FocusSourceHasTransientUserActivation();
  void NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type) override;
  bool Reload() override;
  bool IsDOMContentLoaded() override;
  void UpdateIsAdFrame(bool is_ad_frame) override;
  void SetIsXrOverlaySetup() override;
  ukm::SourceId GetPageUkmSourceId() override;
  StoragePartitionImpl* GetStoragePartition() override;
  BrowserContext* GetBrowserContext() override;
  void ReportInspectorIssue(blink::mojom::InspectorIssueInfoPtr info) override;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) const override;
  void GetCanonicalUrl(
      base::OnceCallback<void(const std::optional<GURL>&)> callback) override;
  void GetOpenGraphMetadata(
      base::OnceCallback<void(blink::mojom::OpenGraphMetadataPtr)> callback)
      override;
  bool IsErrorDocument() const override;
  DocumentRef GetDocumentRef() override;
  WeakDocumentPtr GetWeakDocumentPtr() override;
  void EnableMojoJsBindings(
      content::mojom::ExtraMojoJsFeaturesPtr features) override;
  bool ShouldChangeRenderFrameHostOnSameSiteNavigation() const override;
  bool IsClipboardOwner(ui::ClipboardSequenceNumberToken seqno) const override;
  void MarkClipboardOwner(ui::ClipboardSequenceNumberToken seqno) override;
  bool IsUntrustedNetworkDisabled() const override;
  bool HasPolicyContainerHost() const override;

  // Additional non-override const version of GetMainFrame.
  const RenderFrameHostImpl* GetMainFrame() const;

  // Additional non-override const version of GetPage.
  const PageImpl& GetPage() const;

  // Returns the token for the document currently associated with this frame.
  // This can change over time if a `RenderFrameHost` is reused when navigating
  // to a new document.
  //
  // Retrieving the document token is disallowed if:
  // - the RenderFrameHost is pending commit, e.g. waiting for the renderer to
  //   acknowledge the commit, since the DocumentToken will change as soon as
  //   the navigation actually commits.
  // - the RenderFrameHost is speculative
  const blink::DocumentToken& GetDocumentToken() const;

  // Retrieving the document token is disallowed during times when the result
  // might be misleading / confusing (kPendingCommit or kSpeculative).
  // Internally, the content implementation may still need to retrieve the
  // document token at those times, so provide an escape hatch.
  const blink::DocumentToken& GetDocumentTokenIgnoringSafetyRestrictions()
      const {
    return document_associated_data_->token();
  }

  // Returns a non-null DocumentToken pointer if a cross-document navigation
  // should reuse the DocumentToken. This is only ever the case for the first
  // cross-document commit in a speculative RenderFrameHost. Otherwise, returns
  // nullptr.
  const blink::DocumentToken* GetDocumentTokenForCrossDocumentNavigationReuse(
      base::PassKey<NavigationRequest>);

  // A RenderFrame was previously created but no longer exists, e.g. the
  // renderer process is gone due to a crash.
  bool is_render_frame_deleted() const {
    return render_frame_state_ == RenderFrameState::kDeleted;
  }

  // Has the RenderFrame been created in the renderer process and not yet been
  // deleted, exited or crashed. See RenderFrameState.
  bool is_render_frame_created() const {
    return render_frame_state_ == RenderFrameState::kCreated;
  }

  // Immediately reinitializes DocumentUserData when the RenderFrameHost needs
  // to be immediately reused after a crash. Only usable for a main frame where
  // `is_render_frame_deleted()` is true.
  void ReinitializeDocumentAssociatedDataForReuseAfterCrash(
      base::PassKey<RenderFrameHostManager>);

  // Immediately reinitializes DocumentUserData for testing a corner case crash
  // scenario. See usage in
  // ManifestBrowserTest.GetManifestInterruptedByDestruction.
  void ReinitializeDocumentAssociatedDataForTesting();

  // Determines if a clipboard paste using |data| of type |data_type| is allowed
  // in this renderer frame.  The implementation delegates to
  // RenderFrameHostDelegate::IsClipboardPasteAllowedByPolicy().  See the
  // description of the latter method for complete details.
  void IsClipboardPasteAllowedByPolicy(
      const ClipboardEndpoint& source,
      const ClipboardEndpoint& destination,
      const ClipboardMetadata& metadata,
      ClipboardPasteData clipboard_paste_data,
      IsClipboardPasteAllowedCallback callback);

  // Called when `copied_text` has been copied from this RenderFrameHost to the
  // clipboard. The implementation delegates to
  // RenderFrameHostDelegate::OnTextCopiedToClipboard.
  void OnTextCopiedToClipboard(const std::u16string& copied_text);

  // This is called when accessibility events arrive from renderer to browser.
  // This could cause eviction if the page is in back/forward cache. Returns
  // true if the eviction happens, and otherwise calls
  // |RenderFrameHost::IsInactiveAndDisallowActivation()| and returns the value
  // from there. This is only called when the flag to evict on accessibility
  // events is on. When the flag is off, we do not evict the entry and keep
  // processing the events, thus do not call this function.
  bool IsInactiveAndDisallowActivationForAXEvents(
      const std::vector<ui::AXEvent>& events);

  void SendAccessibilityEventsToManager(const ui::AXUpdatesAndEvents& details);
  void ExerciseAccessibilityForTest();

  // Evict the RenderFrameHostImpl with |reason| that causes the eviction. This
  // constructs a flattened list of NotRestoredReasons and calls
  // |EvictFromBackForwardCacheWithFlattenedReasons|.
  void EvictFromBackForwardCacheWithReason(
      BackForwardCacheMetrics::NotRestoredReason reason);
  // Evict the RenderFrameHostImpl with |can_store_flat| as the eviction reason.
  // This constructs a tree of NotRestoredReasons based on |can_store_flat| and
  // calls |EvictFromBackForwardCacheWithFlattenedAndTreeReasons|.
  void EvictFromBackForwardCacheWithFlattenedReasons(
      BackForwardCacheCanStoreDocumentResult can_store_flat);
  // Evict the RenderFrameHostImpl with |can_store| that causes the eviction.
  // This reports the flattened list and the tree of NotRestoredReasons to
  // metrics, and posts a task to evict the frame.
  void EvictFromBackForwardCacheWithFlattenedAndTreeReasons(
      BackForwardCacheCanStoreDocumentResultWithTree& can_store);

  // Only for testing sticky WebBackForwardCacheDisablingFeature.
  // This is implemented solely in the browser and should only be used when
  // stickiness is required, otherwise
  // BackForwardCacheBrowserTest::AddBlocklistedFeature should be used.
  void UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  const blink::mojom::BackForwardCacheNotRestoredReasonsPtr&
  NotRestoredReasonsForTesting() {
    return not_restored_reasons_for_testing_;
  }

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
  std::string ToDebugString() override;

  // ui::AXPlatformTreeManagerDelegate
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() override;
  float AccessibilityGetDeviceScaleFactor() override;
  void UnrecoverableAccessibilityError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> opt_callback) override;
  gfx::NativeWindow GetTopLevelNativeWindow() override;
  bool CanFireAccessibilityEvents() const override;
  bool AccessibilityIsRootFrame() const override;
  bool ShouldSuppressAXLoadComplete() override;
  WebContentsAccessibility* AccessibilityGetWebContentsAccessibility() override;

  // ui::AXNodeIdDelegate:
  ui::AXPlatformNodeId GetOrCreateAXNodeUniqueId(
      ui::AXNodeID ax_node_id) override;
  void OnAXNodeDeleted(ui::AXNodeID ax_node_id) override;

  // SiteInstanceGroup::Observer
  void RenderProcessGone(SiteInstanceGroup* site_instance_group,
                         const ChildProcessTerminationInfo& info) override;

  // ui::AXActionHandlerBase:
  void PerformAction(const ui::AXActionData& data) override;
  bool RequiresPerformActionPointInPixels() const override;

  // Creates a RenderFrame in the renderer process.
  bool CreateRenderFrame(
      const std::optional<blink::FrameToken>& previous_frame_token,
      const std::optional<blink::FrameToken>& opener_frame_token,
      const std::optional<blink::FrameToken>& parent_frame_token,
      const std::optional<blink::FrameToken>& previous_sibling_frame_token);

  // Deletes the RenderFrame in the renderer process.
  // Postcondition: |IsPendingDeletion()| is true.
  void DeleteRenderFrame(mojom::FrameDeleteIntention intent);

  // Track whether the RenderFrame for this RenderFrameHost has been created in
  // or destroyed in the renderer process.
  void RenderFrameCreated();
  void RenderFrameDeleted();

  // Signals that the renderer has requested for this main-frame's window to be
  // shown, at which point we can service navigation requests.
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

  // Adds/removes a media stream to this render frame.
  enum MediaStreamType {
    // This stream is capturing an audio or video source. For example, the
    // stream could be capturing the microphone's audio, or capturing the
    // content of a tab.
    kCapturingMediaStream,
    // This stream is playing audio data. For example, the user is listening to
    // an audio file. Exists even if the audio data is silent.
    kPlayingAudioStream,
    // Same as kPlayingAudioStream, but only if the audio data is actually
    // audible to the user.
    kPlayingAudibleAudioStream,
    kCount,
  };
  void OnMediaStreamAdded(MediaStreamType type);
  void OnMediaStreamRemoved(MediaStreamType type);

  // Returns true if this frame has at least one media stream of this `type`.
  bool HasMediaStreams(MediaStreamType type) const;

  // Returns the MediaStreamType that denotes if the frame should be considered
  // audible. This returns kPlayingAudibleAudioStream on all platforms except
  // for MacOS. On MacOS, all active audio stream are considered as audible. Thi
  // avoid a priority inversion that causes audio glitches when a renderer
  // playing silent audio is backgrounded. See https://crbug.com/1504625.
  static constexpr MediaStreamType GetAudibleMediaStreamType() {
#if BUILDFLAG(IS_MAC)
    return MediaStreamType::kPlayingAudioStream;
#else
    return MediaStreamType::kPlayingAudibleAudioStream;
#endif
  }

  // Called when this frame has added a child. This is a continuation of an IPC
  // that was partially handled on the IO thread (to allocate |new_routing_id|,
  // |frame_token|, |devtools_frame_token|, |document_token|), and is forwarded
  // here. The renderer has already been told to create a RenderFrame with the
  // specified ID values. |browser_interface_broker_receiver| is the receiver
  // end of the BrowserInterfaceBroker interface in the child frame.
  // RenderFrameHost should bind this receiver to expose services to the
  // renderer process. The caller takes care of sending down the client end of
  // the pipe to the child RenderFrame to use.
  void OnCreateChildFrame(
      int new_routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          associated_interface_provider_receiver,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const blink::LocalFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const blink::DocumentToken& document_token,
      const blink::FramePolicy& frame_policy,
      const blink::mojom::FrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType owner_type,
      ukm::SourceId document_ukm_source_id);

  void OnPreloadingHeuristicsModelDone(const GURL& url, float score) override;

  // Update this frame's state at the appropriate time when a navigation
  // commits. This is called by Navigator::DidNavigate as a helper, in the
  // midst of a DidCommitProvisionalLoad call. If |was_within_same_document| is
  // true the navigation was same-document.
  void DidNavigate(const mojom::DidCommitProvisionalLoadParams& params,
                   NavigationRequest* navigation_request,
                   bool was_within_same_document);

  RenderViewHostImpl* render_view_host() { return render_view_host_.get(); }
  RenderFrameHostDelegate* delegate() { return delegate_; }

  // FrameTree references are being removed from RenderFrameHostImpl as a part
  // of MPArch. Please avoid using these APIs. See crbug.com/1179502 for
  // details.
  FrameTree* frame_tree() const { return frame_tree_; }
  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  // Methods to add/remove/reset/query child FrameTreeNodes of this frame.
  // See class-level comment for FrameTreeNode for how the frame tree is
  // represented.
  size_t child_count() { return children_.size(); }
  FrameTreeNode* child_at(size_t index) const { return children_[index].get(); }
  FrameTreeNode* AddChild(
      std::unique_ptr<FrameTreeNode> child,
      int frame_routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      const blink::LocalFrameToken& frame_token,
      const blink::DocumentToken& document_token,
      base::UnguessableToken devtools_frame_token,
      const blink::FramePolicy& frame_policy,
      std::string frame_name,
      std::string frame_unique_name);
  void RemoveChild(FrameTreeNode* child);
  void ResetChildren();

  // Set the URL of the document represented by this RenderFrameHost. Called
  // when the navigation commits. See also `GetLastCommittedURL`.
  void SetLastCommittedUrl(const GURL& url);

  // RenderFrameHost represents a document in a frame. It is either:
  // 1. The initial empty document,
  // 2. A document created by a navigation.
  //
  // In case of (2), this returns the ID of the navigation who created this
  // document.
  //
  // Note 1: This is updated after receiving DidCommitNavigation IPC.
  // Note 2: Same-document navigation are not updating this field.
  int64_t navigation_id() const { return navigation_id_; }

  // The most recent non-net-error URL to commit in this frame.  In almost all
  // cases, use GetLastCommittedURL instead.
  const GURL& last_successful_url() const { return last_successful_url_; }

  // For about:blank and about:srcdoc documents, this tracks the inherited base
  // URL, snapshotted from the initiator's FrameLoadRequest. This is an empty
  // URL for all other cases.
  const GURL& GetInheritedBaseUrl() const;

  // The current URL of the document in the renderer process. Note that this
  // includes URL updates due to document.open() (where it will be updated to
  // the document-open-initiator URL) and special cases like error pages
  // (where kUnreachableWebDataURL is used) and loadDataWithBaseURL() (where the
  // base URL is used), so it might be different than the "last committed
  // URL" provided by GetLastCommittedURL().
  // In almost all cases, use GetLastCommittedURL() instead, as this should only
  // be used when the caller wants to know the current state of the URL in the
  // renderer (e.g. when predicting whether a navigation will do a replacement
  // or not).
  const GURL& last_document_url_in_renderer() const {
    return renderer_url_info_.last_document_url;
  }

  // Whether the last committed document was loaded from loadDataWithBaseURL or
  // not.
  bool was_loaded_from_load_data_with_base_url() const {
    return renderer_url_info_.was_loaded_from_load_data_with_base_url;
  }

  // Saves the URLs and other URL-related information used in the renderer.
  // These values can be used to know the current state of URLs in the renderer.
  // Currently these values are used to simulate calculations in the renderer
  // or to preserve behavior of calculations that used to live in the renderer
  // but was moved to the browser. For most use cases, prefer to use
  // `last_committed_url_` instead.
  struct RendererURLInfo {
    // Tracks this frame's last "document URL", which might be different from:
    // - `last_committed_url_` if the frame did document.open() or sets a
    // different document URL than the committed URL (e.g. loadDataWithBaseURL
    // and error page commits).
    // - The history URL in the renderer (not tracked in the browser) which
    // might be different for error pages, where the document URL will be
    // kUnreachableWebDataURL.
    // Note 1: `last_document_url` might be updated outside of navigation due
    // to document.open(), unlike `last_committed_url_` which can only be
    // updated as a result of navigation. All three URLs are also updated/set to
    // empty when the renderer process crashes.
    // Note 2: This might not have the accurate value of the document URL in the
    // renderer after same-document navigations on error pages or documents
    // loaded through loadDataWithBaseURL(). See comment in GetLastDocumentURL()
    // in render_frame_host_impl.cc for more details.
    GURL last_document_url;

    // Whether the currently committed document is a result of webview's
    // loadDataWithBaseURL API or not.
    bool was_loaded_from_load_data_with_base_url = false;
  };

  // Returns the http method of the last committed navigation.
  const std::string& last_http_method() { return last_http_method_; }

  // Returns the http status code of the last committed navigation.
  int last_http_status_code() { return last_http_status_code_; }

  // Returns the POST ID of the last committed navigation.
  int64_t last_post_id() { return last_post_id_; }

  // Returns true if last committed navigation's CommonNavigationParam's
  // `has_user_gesture` is true. Should only be used to get the state of the
  // last navigation, and not the current state of user activation of this
  // RenderFrameHost. See comment on the variable declaration for more details.
  bool last_committed_common_params_has_user_gesture() const {
    return last_committed_common_params_has_user_gesture_;
  }

  // Returns true if `dest_url_info` should be considered the same site as the
  // current contents of this frame. This is the primary entry point for
  // determining if a navigation to `dest_url_info` should stay in this
  // RenderFrameHost's SiteInstance.
  bool IsNavigationSameSite(const UrlInfo& dest_url_info) const;

  // Returns |frame_origin| if this frame is the top (i.e. root) frame in the
  // frame tree. Otherwise, it returns the top frame's origin.
  const url::Origin& ComputeTopFrameOrigin(
      const url::Origin& frame_origin) const;

  // Computes the IsolationInfo for this frame to `destination`. Set
  // `is_credentialless` to true if the navigation will be loaded as a
  // credentialless document (note that the navigation might be committing a
  // credentialless document even if the document currently loaded in this RFH
  // is not credentialless, and vice versa). Populate
  // `fenced_frame_nonce_for_navigation` with
  // `NavigationRequest::ComputeFencedFrameNonce()`.
  net::IsolationInfo ComputeIsolationInfoForNavigation(
      const GURL& destination,
      bool is_credentialless,
      std::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation);

  // Computes the IsolationInfo for this frame to |destination|.
  net::IsolationInfo ComputeIsolationInfoForNavigation(const GURL& destination);

  // Computes the IsolationInfo that should be used for subresources, if
  // |main_world_origin_for_url_loader_factory| is committed to this frame. The
  // boolean `is_credentialless` specifies whether this frame will commit an
  // credentialless document.
  // Populate `fenced_frame_nonce_for_navigation` with
  // `NavigationRequest::ComputeFencedFrameNonce()`.
  net::IsolationInfo ComputeIsolationInfoForSubresourcesForPendingCommit(
      const url::Origin& main_world_origin_for_url_loader_factory,
      bool is_credentialless,
      std::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation);

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

  // Get HTML data for this RenderFrame by serializing contents on the renderer
  // side and replacing all links to both same-site and cross-site resources
  // with paths to local copies as specified by |url_map| and |frame_token_map|.
  void GetSerializedHtmlWithLocalLinks(
      const base::flat_map<GURL, base::FilePath>& url_map,
      const base::flat_map<blink::FrameToken, base::FilePath>& frame_token_map,
      bool save_with_empty_url,
      mojo::PendingRemote<mojom::FrameHTMLSerializerHandler>
          serializer_handler);

  // Returns the associated WebUI or null if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }
  WebUI::TypeID web_ui_type() const { return web_ui_type_; }

  // Enable Mojo JavaScript bindings in the renderer process, and use the
  // provided BrowserInterfaceBroker to handle JavaScript calls to
  // Mojo.bindInterface. This method should be called in
  // ReadyToCommitNavigation.
  void EnableMojoJsBindingsWithBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker);

  // Frame trees may be nested so it can be the case that is_main_frame() is
  // true, but is not the outermost RenderFrameHost (it only checks for nullity
  // of |parent_|. In particular, !is_main_frame() cannot be used to check if
  // this RenderFrameHost is embedded -- use !IsOutermostMainFrame() instead.
  // NB: this does not escape guest views; IsOutermostMainFrame() will be true
  // for the outermost main frame in an inner guest view.
  bool is_main_frame() const { return !parent_; }
  bool IsOutermostMainFrame() const;

  // Returns this RenderFrameHost's loading state. This method is only used by
  // FrameTreeNode. The proper way to check whether a frame is loading is to
  // call FrameTreeNode::IsLoading.
  bool is_loading() const { return loading_state_ != LoadingState::NONE; }

  LoadingState loading_state() const { return loading_state_; }

  // Sets `loading_state_` to LOADING_UI_REQUESTED to handle renderer debug
  // URLs. This is needed to generate DidStopLoading events for these URLs.
  void SetIsLoadingForRendererDebugURL();
  // Returns true if this is a top-level frame, or if this frame
  // uses a proxy to communicate with its parent frame. Local roots are
  // distinguished by owning a RenderWidgetHost, which manages input events
  // and painting for this frame and its contiguous local subtree in the
  // renderer process.
  bool is_local_root() const { return !!GetLocalRenderWidgetHost(); }

  // Returns true if this is not a top-level frame but is still a local root.
  bool is_local_root_subframe() const {
    return !is_main_frame() && is_local_root();
  }

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.
  int nav_entry_id() const { return nav_entry_id_; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

  // The FrameNavigationEntry for the current document in this RenderFrameHost.
  // See last_committed_frame_entry_ declaration for more details.
  FrameNavigationEntry* last_committed_frame_entry() {
    return last_committed_frame_entry_.get();
  }
  void set_last_committed_frame_entry(
      scoped_refptr<FrameNavigationEntry> frame_entry) {
    last_committed_frame_entry_ = frame_entry;
  }

  // Returns the SiteInfo derived from the UrlInfo of the last navigation that
  // committed in this RenderFrameHost.
  // Note that this might be different from GetSiteInstance()->GetSiteInfo().
  // See comment in the member declaration for more details.
  const SiteInfo& last_committed_url_derived_site_info() {
    return last_committed_url_derived_site_info_;
  }

  // Return true if this contains at least one NavigationRequest waiting to
  // commit in this RenderFrameHost. This includes both same-document and
  // cross-document NavigationRequests.
  bool HasPendingCommitNavigation() const;

  // Return true if this contains at least one NavigationRequest waiting to
  // commit in this RenderFrameHost, excluding same-document navigations.
  // NOTE: RenderFrameHostManager surfaces a similar method that will check
  // all the RenderFrameHosts for that FrameTreeNode.
  bool HasPendingCommitForCrossDocumentNavigation() const;

  // Helper for recording metrics when another NavigationRequest is blocked by
  // navigation queueing, i.e. because this NavigationRequest is in the process
  // of committing a navigation in a kPendingCommit RenderFrameHost.
  // `commit_attempt` should be true if the other NavigationRequest was also
  // trying to commit a navigation.
  void RecordMetricsForBlockedGetFrameHostAttempt(bool commit_attempt);

  // Return true if Unload() was called on the frame or one of its ancestors.
  // If true, this corresponds either to unload handlers running for this
  // RenderFrameHost (LifecycleStateImpl::kRunningUnloadHandlers) or when this
  // RenderFrameHost is ready to be deleted
  // (LifecycleStateImpl::kReadyToBeDeleted).
  bool IsPendingDeletion() const;

  // Returns true if this RenderFrameHost is currently stored in the
  // back-forward cache i.e., when lifecycle_state() is kInBackForwardCache.
  bool IsInBackForwardCache() const;

  // Returns a pending same-document navigation request in this frame that has
  // the navigation_token |token|, if any.
  NavigationRequest* GetSameDocumentNavigationRequest(
      const base::UnguessableToken& token);

  // Resets the NavigationRequests stored in this RenderFrameHost, which are all
  // "pending commit". Note this won't affect navigations that are "pending
  // commit" but not owned by this RenderFrameHost, and any navigation that
  // hasn't reached the "pending commit" stage yet, which would still be owned
  // by the FrameTreeNode.
  // TODO(crbug.com/40186427): Don't allow this to be called when there
  // are pending cross-document navigations except for FrameTreeNode detach,
  // RFH destruction, or when the renderer process is gone, so that we don't
  // have to "undo" the commit that already happens in the renderer.
  void ResetOwnedNavigationRequests(NavigationDiscardReason reason);

  // Called when a navigation is ready to commit in this
  // RenderFrameHost. Transfers ownership of the NavigationRequest associated
  // with the navigation to this RenderFrameHost.
  void SetNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  const scoped_refptr<NavigationOrDocumentHandle>&
  GetNavigationOrDocumentHandle();

  // Tells the renderer that this RenderFrame is being replaced with one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // `blink::RemoteFrame` to replace the RenderFrame and set it to `is_loading`
  // state. The renderer process keeps the `blink::RemoteFrame` object around as
  // a placeholder while the frame is rendered in a different process.
  //
  // There should always be a `proxy` to replace the old RenderFrameHost. If
  // there are no remaining active views in the process, the proxy will be
  // short-lived and will be deleted when the unload ACK is received.
  //
  // RenderDocument: After a local<->local swap, this function is called with a
  // null |proxy|. It executes common cleanup and marks this RenderFrameHost to
  // have completed its unload handler. The RenderFrameHost may be immediately
  // deleted or deferred depending on its children's unload status.
  void Unload(RenderFrameProxyHost* proxy, bool is_loading);

  // Sent to a renderer when the browser needs to cancel a navigation associated
  // with a speculative RenderFrameHost that has already been asked to commit
  // via `CommitNavigation()`. The renderer will swap out the already-committed
  // RenderFrame, replacing it with a `blink::RemoteFrame` for `proxy`.
  //
  // TODO(crbug.com/40186427): This method is fundamentally incompatible
  // with RenderDocument, as there is no `blink::RemoteFrame` to restore for a
  // local<->local swap.
  void UndoCommitNavigation(RenderFrameProxyHost& proxy, bool is_loading);

  // Unload this frame for the proxy. Similar to `Unload()` but without
  // managing the lifecycle of this object.
  void SwapOuterDelegateFrame(RenderFrameProxyHost* proxy);

  // Process the acknowledgment of the unload of this frame from the renderer.
  void OnUnloadACK();

  // Depending on switches, may log when an unload ack has not been received.
  void MaybeLogMissingUnloadAck();

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
  // NOTE: this must be kept consistent with the
  // RenderFrameHostImpl.LifecycleState enum in chrome_track_event.proto for
  // tracing.
  enum class LifecycleStateImpl {
    // This state corresponds to when a speculative RenderFrameHost is created
    // for an ongoing navigation (to new URL) but the navigation hasn't reached
    // ReadyToCommitNavigation stage yet, mainly created for performance
    // optimization. The frame can only be created in this state and no
    // transitions happen to this state.
    //
    // Transitions from this state happen to one of:
    // - kPendingCommit - when cross-RenderFrameHost navigation commits in
    // the renderer and becomes ready to commit and the //content embedders are
    // notified about the navigation's association with this RenderFrameHost.
    // - kActive -- when speculative RenderFrameHost is swapped indirectly
    // instead of following the full navigation path (known as "early commit").
    // This happens when current RenderFrameHost is not live. The work to
    // remove this transition is tracked in crbug.com/1072817.
    //
    // Speculative RenderFrameHost deletion happens without running any unload
    // handlers and with LifecycleStateImpl remaining in kSpeculative state.
    //
    // Note that the term speculative is used, because the navigation might be
    // canceled or redirected and the RenderFrameHost might get deleted before
    // being used.
    kSpeculative,

    // This state corresponds to when a cross-RenderFrameHost navigation is
    // waiting for an acknowledgment from the renderer to swap the
    // RenderFrameHost.
    //
    // Note that cross-document same-RenderFrameHost navigations are not covered
    // by this state, despite going through ReadyToCommitNavigation (the
    // RenderFrameHost will be considered current and be in either kActive or
    // kPrerendering state). The work to eliminate cross-document
    // same-RenderFrameHost navigations is tracked in crbug.com/936696.
    //
    // Transitions from this state happen to one of:
    // - kActive -- when a cross-RenderFrameHost navigation commits inside
    // the primary frame tree.
    // - kPrerendering -- when a cross-RenderFrameHost navigation commits
    // inside prerendered frame tree.
    // - kReadyToBeDeleted -- when the navigation gets aborted. The work to
    // eliminate this is tracked in crbug.com/999255.
    //
    // Transition to this state only happens from kSpeculative state when a
    // speculative RenderFrameHost created for cross-RenderFrameHost navigation
    // commits in the renderer.
    kPendingCommit,

    // Prerender2:
    // This state corresponds to when a RenderFrameHost is the current one in
    // its RenderFrameHostManager and FrameTreeNode for a prerendered frame
    // tree. Documents in this state are invisible to the user and aren't
    // allowed to show any UI changes, but the page is allowed to load and run
    // in the background. Documents in kPrerendering state can be evicted
    // (cancelling prerendering) at any time.
    //
    // A prerendered page is created by an initial navigation in a prerendered
    // frame tree. For the prerendered page to be shown to the user, another
    // navigation in the primary frame tree activates the prerendered page.
    //
    // Transitions from this state happen to one of:
    // - kActive -- when the prerendered page is activated.
    // - kRunningUnloadHandlers -- when a navigation commits in a prerendered
    // frame tree, unloading the previous one.
    // - kReadyToBeDeleted -- when prerendering is cancelled and the prerendered
    // page is deleted.
    //
    // Document can be created in kPrerendering state (while initializing root
    // and child in a prerendered frame tree).
    //
    // Transition to kPrerendering can happen from kPendingCommit (when
    // cross-RenderFrameHost navigation commits inside a prerendered frame
    // tree).
    //
    // Please note that Prerender2 is an experimental feature behind the flag.
    //
    // Note that at the moment, this state is *not* used for RenderFrameHosts in
    // nested FrameTrees inside prerendered pages. See crbug.com/1232528,
    // crbug.com/1244274 for more discussion on whether or not we should support
    // nested FrameTrees inside prerendered pages.
    kPrerendering,

    // This state corresponds to when a RenderFrameHost is the current one in
    // its RenderFrameHostManager/FrameTreeNode inside a primary FrameTree or
    // its descendant FrameTrees. In this state, RenderFrameHost is visible to
    // the user. TODO(crbug.com/1232528, crbug.com/1244274): At the moment,
    // prerendered pages implicitly support nested frame trees, whose
    // RenderFrameHost's are always kActive even though they are not shown to
    // the user. We need to formally determine if prerenders should support
    // nested FrameTrees.
    //
    // Transition to kActive state may happen from one of:
    // - kSpeculative -- when a speculative RenderFrameHost commits to make it
    // the current one in primary frame tree before the corresponding navigation
    // commits.
    // - kPendingCommit -- when a cross-RenderFrameHost navigation commits. The
    // work to eliminate these early commits is tracked in crbug.com/936696
    // - kInBackForwardCache -- when restoring from BackForwardCache.
    // - kPrerendering -- when a prerendered page activates.
    //
    // RenderFrameHost can also be created in this state for an empty document
    // in a FrameTreeNode (e.g initializing root and child in an empty
    // primary FrameTree).
    //
    // Note that this state is also used for nested pages e.g., the
    // RenderFrameHosts in <fencedframe> elements, as these nested contexts do
    // not get their own lifecycle state. A RenderFrameHost can tell if it is in
    // a <fencedframe> however, by checking its `FrameTree`'s type.
    kActive,

    // This state corresponds to when RenderFrameHost is stored in
    // BackForwardCache. This happens when the user navigates away from a
    // document, so that the RenderFrameHost can be re-used after a history
    // navigation. Transition to this state happens only from kActive state.
    // BackForwardCache is disabled in prerendering frame trees because a
    // prerendered page is invisible, and the user can't perform any
    // back/forward navigations.
    kInBackForwardCache,

    // This state corresponds to when RenderFrameHost has started running unload
    // handlers (this includes handlers for the "unload", "pagehide", and
    // "visibilitychange" events). An event such as navigation commit or
    // detaching the frame causes the RenderFrameHost to transition to this
    // state. Then, the RenderFrameHost sends IPCs to the renderer process to
    // execute unload handlers and deletes the RenderFrame. The RenderFrameHost
    // waits for an ACK from the renderer process, either
    // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame for a navigating
    // frame or FrameHostMsg_Detach for its subframes, after which the
    // RenderFrameHost transitions to kReadyToBeDeleted state.
    //
    // Transition to this state happens only from kActive and kPrerendering
    // states. Note that eviction from BackForwardCache does not wait for unload
    // handlers, and kInBackForwardCache moves to kReadyToBeDeleted.
    // TODO(crbug.com/40187396): Omit unload handling on canceling
    // prerendering, and making kPrerendering move to kReadyToBeDeleted
    // directly.
    kRunningUnloadHandlers,

    // This state corresponds to when RenderFrameHost has completed running the
    // unload handlers. Once all the descendant frames in other processes are
    // gone, this RenderFrameHost will delete itself. Transition to this state
    // may happen from one of kPrerendering, kActive, kInBackForwardCache or
    // kRunningUnloadHandlers states.
    kReadyToBeDeleted,
  };
  // Returns the string corresponding to LifecycleStateImpl, used for logging
  // crash keys.
  static const char* LifecycleStateImplToString(LifecycleStateImpl state);

  LifecycleStateImpl lifecycle_state() const { return lifecycle_state_; }

  // Updates the `lifecycle_state_`. This will also notify the delegate
  // about `RenderFrameHostStateChanged` when the old and new
  // `RenderFrameHost::LifecycleState` changes.
  //
  // When the `new_state == LifecycleStateImpl::kActive`, LifecycleStateImpl of
  // RenderFrameHost and all its children are also updated to
  // `LifecycleStateImpl::kActive`.
  void SetLifecycleState(LifecycleStateImpl new_state);

  // Sets |has_pending_lifecycle_state_update_| to true for this
  // RenderFrameHost and its children. Called when this RenderFrameHost stops
  // being the current one in the RenderFrameHostManager, but its new
  // LifecycleStateImpl is not immediately determined. This boolean is reset
  // when this RenderFrameHost enters the back-forward-cache or the pending
  // deletion list.
  // `last_frame_type` is the type of the frame from which this RenderFrameHost
  // was just unset. This is null when called recursively for the children.
  void SetHasPendingLifecycleStateUpdate(
      std::optional<FrameType> last_frame_type);

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

#if BUILDFLAG(IS_ANDROID)
  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
  using ExtractSmartClipDataCallback = base::OnceCallback<
      void(const std::u16string&, const std::u16string&, const gfx::Rect&)>;

  void RequestSmartClipExtract(ExtractSmartClipDataCallback callback,
                               gfx::Rect rect);

  void OnSmartClipDataExtracted(int32_t callback_id,
                                const std::u16string& text,
                                const std::u16string& html,
                                const gfx::Rect& clip_rect);
#endif  // BUILDFLAG(IS_ANDROID)

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode.
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                             mojom::SnapshotAccessibilityTreeParamsPtr params);

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

  // Updating focus in the presence of multiple frame trees requires multiple
  // focus changes. The existence of this class will defer UpdateAXTreeData()
  // until this process has finished and the focus states are consistent.
  class UpdateAXFocusDeferScope {
   public:
    explicit UpdateAXFocusDeferScope(RenderFrameHostImpl& rfh);
    ~UpdateAXFocusDeferScope();

   private:
    const base::SafeRef<RenderFrameHostImpl> rfh_;
  };

  // Access the BrowserAccessibilityManager if it already exists.
  ui::BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // null.
  ui::BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

  // Indicates that this process wants the |untrusted_stack_trace| parameter of
  // FrameHost.DidAddMessageToConsole() to be filled in as much as possible for
  // log_level == kError messages.
  void SetWantErrorMessageStackTrace();

  // Listens to the change events of the cookies associated with the domain of
  // the specified URL during initialization. It also contains the information
  // of whether any cookie/HTTPOnly cookie had been changed before, which can be
  // used to determine if a document with Cache-control: no-store header set is
  // eligible for BFCache.
  class CookieChangeListener : public network::mojom::CookieChangeListener {
   public:
    struct CookieChangeInfo {
      // The number of observed cookie modifications.
      int64_t cookie_modification_count_ = 0;
      // The number of observed HTTPOnly cookie modifications.
      int64_t http_only_cookie_modification_count_ = 0;
    };

    CookieChangeListener(StoragePartition* storage_partition, GURL& url);
    ~CookieChangeListener() override;
    CookieChangeListener(const CookieChangeListener&) = delete;
    CookieChangeListener& operator=(const CookieChangeListener&) = delete;

    // Returns a copy of the `cookie_change_info_`.
    CookieChangeInfo cookie_change_info() { return cookie_change_info_; }

    // We don't want to count the cookie modification made by the
    // `NavigationRequest` itself, so provide this function to allow the count
    // adjustment.
    // Passing the `base::PassKey` to restrict the caller of this method to
    // `NavigationRequest` only.
    void RemoveNavigationCookieModificationCount(
        base::PassKey<content::NavigationRequest> navigation_request,
        uint64_t cookie_modification_count_delta,
        uint64_t http_only_cookie_modification_count_delta) {
      cookie_change_info_.cookie_modification_count_ -=
          cookie_modification_count_delta;
      cookie_change_info_.http_only_cookie_modification_count_ -=
          http_only_cookie_modification_count_delta;
    }

   private:
    // network::mojom::CookieChangeListener
    void OnCookieChange(const net::CookieChangeInfo& change) override;

    mojo::Receiver<network::mojom::CookieChangeListener>
        cookie_change_listener_receiver_{this};

    CookieChangeInfo cookie_change_info_;
  };

  // Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  // |subresource_loader_params| is used in network service land to pass
  // the parameters to create a custom subresource loader in the renderer
  // process.
  void CommitNavigation(
      NavigationRequest* navigation_request,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      const std::optional<blink::DocumentToken>& document_token,
      const base::UnguessableToken& devtools_navigation_token);

  // Indicates that a navigation failed and that this RenderFrame should display
  // an error page.
  void FailedNavigation(
      NavigationRequest* navigation_request,
      const blink::mojom::CommonNavigationParams& common_params,
      const blink::mojom::CommitNavigationParams& commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      int extended_error_code,
      const std::optional<std::string>& error_page_content,
      const blink::DocumentToken& document_token);

  void AddResourceTimingEntryForFailedSubframeNavigation(
      FrameTreeNode* child_frame,
      base::TimeTicks start_time,
      base::TimeTicks redirect_time,
      const GURL& initial_url,
      const GURL& final_url,
      network::mojom::URLResponseHeadPtr response_head,
      bool allow_response_details,
      const network::URLLoaderCompletionStatus& completion_status);

  // Sends a renderer-debug URL to the renderer process for handling.
  void HandleRendererDebugURL(const GURL& url);

  // Requests that the renderer discard the frame associated with this host,
  // freeing up as many resources as possible.
  void DiscardFrame();

  // BEGIN IPC REVIEW BOUNDARY: to enforce security review for IPC, these
  // methods are defined in render_frame_host_impl_interface_bindings.cc.

  // Similar to the public `CreateMessageFilterForAssociatedReceiver()` but
  // allows a specific message handling policy to be specified.
  std::unique_ptr<mojo::MessageFilter>
  CreateMessageFilterForAssociatedReceiverInternal(
      const char* interface_name,
      BackForwardCacheImpl::MessageHandlingPolicyWhenCached policy);

  // Sets up the Mojo connection between this instance and its associated render
  // frame.
  void SetUpMojoConnection();

  // Tears down the browser-side state relating to the Mojo connection between
  // this instance and its associated render frame.
  void TearDownMojoConnection();

  // END IPC REVIEW BOUNDARY

  // Returns whether the frame is focused. A frame is considered focused when it
  // is the parent chain of the focused frame within the frame tree. In
  // addition, its associated RenderWidgetHost has to be focused.
  bool IsFocused();

  // Sets the WebUI owned by `request` as the WebUI for this RenderFrameHost,
  // which is based on the provided `request`'s URL.
  void SetWebUI(NavigationRequest& request);

  // Destroys WebUI instance and resets related data.
  // This indirectly calls content's embedders and may have arbitrary side
  // effect, like deleting `this`.
  void ClearWebUI();

  // Returns the Mojo ImageDownloader service.
  const mojo::Remote<blink::mojom::ImageDownloader>& GetMojoImageDownloader();

  // Returns remote to renderer side FindInPage associated with this frame.
  const mojo::AssociatedRemote<blink::mojom::FindInPage>& GetFindInPage();

  // Returns associated remote for the blink::mojom::LocalFrame Mojo interface.
  const mojo::AssociatedRemote<blink::mojom::LocalFrame>&
  GetAssociatedLocalFrame();

  // Returns the AgentSchedulingGroupHost associated with this
  // RenderFrameHostImpl.
  virtual AgentSchedulingGroupHost& GetAgentSchedulingGroup();

  // Returns associated remote for the blink::mojom::LocalMainFrame Mojo
  // interface. May be overridden by subclasses, e.g. tests which wish to
  // intercept outgoing local main frame messages.
  virtual blink::mojom::LocalMainFrame* GetAssociatedLocalMainFrame();

  // Returns associated remote for the blink::mojom::FrameBindingsControl Mojo
  // interface.
  const mojo::AssociatedRemote<mojom::FrameBindingsControl>&
  GetFrameBindingsControl();

  // Resets the loading state. Following this call, the RenderFrameHost will be
  // in a non-loading state.
  void ResetLoadingState();

  // Returns the permissions policy which should be enforced on this
  // RenderFrame.
  const blink::PermissionsPolicy* permissions_policy() const {
    return permissions_policy_.get();
  }

  void ClearFocusedElement();

  bool has_focused_editable_element() const {
    return has_focused_editable_element_;
  }

  bool has_focused_richly_editable_element() const {
    return has_focused_richly_editable_element_;
  }

  // Binds a DevToolsAgent interface for debugging.
  void BindDevToolsAgent(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHost() override;
  service_manager::InterfaceProvider* GetJavaInterfaces() override;
#endif

  // Propagates the visibility state along the immediate local roots by calling
  // RenderWidgetHostViewChildFrame::Show()/Hide(). Calling this on a pending
  // or speculative RenderFrameHost (that has not committed) should be avoided.
  void SetVisibilityForChildViews(bool visible);

  const blink::LocalFrameToken& GetTopFrameToken();

  // Returns an unguessable token for this RFHI.  This provides a temporary way
  // to identify a RenderFrameHost that's compatible with IPC.  Else, one needs
  // to send pid + RoutingID, but one cannot send pid.  One can get it from the
  // channel, but this makes it much harder to get wrong.
  // Once media switches to mojo, we should be able to remove this in favor of
  // sending a mojo overlay factory. Note that this value should only be shared
  // with the renderer process that hosts the frame and other utility processes;
  // it should specifically *not* be shared with any renderer processes that
  // do not host the frame.
  const base::UnguessableToken& GetOverlayRoutingToken() const {
    return frame_token_.value();
  }

  // Binds the receiver end of the BrowserInterfaceBroker interface through
  // which services provided by this RenderFrameHost are exposed to the
  // corresponding RenderFrame. The caller is responsible for plumbing the
  // client end to the renderer process.
  void BindBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>);

  // Binds the receiver end of the `AssociatedInterfaceProvider` interface. This
  // is called whenever we generate a remote/receiver pair for this interface
  // and the remote end gets passed to the renderer.
  void BindAssociatedInterfaceProviderReceiver(
      mojo::PendingAssociatedReceiver<
          blink::mojom::AssociatedInterfaceProvider>);

  // Binds the receiver end of the DomOperationControllerHost interface through
  // which services provided by this RenderFrameHost are exposed to the
  // corresponding RenderFrame. The caller is responsible for plumbing the
  // client end to the renderer process.
  void BindDomOperationControllerHostReceiver(
      mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
          receiver);

  // Expose Mojo receivers to tests for use with
  // `mojo::test::ScopedSwapImplForTesting`.
  mojo::AssociatedReceiver<blink::mojom::BackForwardCacheControllerHost>&
  back_forward_cache_controller_host_receiver_for_testing() {
    return back_forward_cache_controller_host_associated_receiver_;
  }
  mojo::AssociatedReceiver<mojom::FrameHost>&
  frame_host_receiver_for_testing() {
    return frame_host_associated_receiver_;
  }
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>&
  local_frame_host_receiver_for_testing() {
    return local_frame_host_receiver_;
  }
  mojo::AssociatedReceiver<blink::mojom::LocalMainFrameHost>&
  local_main_frame_host_receiver_for_testing() {
    return local_main_frame_host_receiver_;
  }
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>&
  browser_interface_broker_receiver_for_testing() {
    return broker_receiver_;
  }
  void SetKeepAliveTimeoutForTesting(base::TimeDelta timeout);

  network::mojom::WebSandboxFlags active_sandbox_flags() {
    CHECK(policy_container_host_)
        << LifecycleStateImplToString(lifecycle_state_);
    return policy_container_host_->sandbox_flags();
  }
  bool is_mhtml_document() const { return is_mhtml_document_; }

  // Returns whether this document is a subframe of a MHTML document.
  bool IsMhtmlSubframe() const;

  ReloadType reload_type() { return reload_type_; }

  // Notifies the render frame that |frame_tree_node_| has received user
  // activation. May be invoked multiple times. This is called both for the
  // actual frame that saw user activation and any ancestor frames that might
  // also be activated as part of UserActivationV2 requirements. Does not
  // include frames activated by the same-origin visibility heuristic, see
  // `UserActivationState` for details.
  void DidReceiveUserActivation();

  // Called when successful web authn assertion occurred in this
  // RenderFrameHost.
  void WebAuthnAssertionRequestSucceeded();

  // Apply any isolation policies, such as site isolation triggered by COOP
  // headers, that might be triggered when a particular frame has just seen a
  // user activation. Called whenever this frame sees a user activation (which
  // may or may not be the first activation in this frame).
  void MaybeIsolateForUserActivation();

  // Returns the current size for this frame.
  const std::optional<gfx::Size>& frame_size() const { return frame_size_; }

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

  // Start a timer that will evict this RenderFrameHost from the
  // BackForwardCache after time to live.
  void StartBackForwardCacheEvictionTimer();

  bool IsBackForwardCacheDisabled() const;

  // Prevents this frame (along with its parents/children) from being added to
  // the BackForwardCache. If the frame is already in the cache an eviction is
  // triggered.
  // For what `source_id` means and when it's set, see `DisabledReasonsMap` from
  // `back_forward_cache_can_store_document_result.h`.
  void DisableBackForwardCache(
      BackForwardCache::DisabledReason reason,
      std::optional<ukm::SourceId> source_id = std::nullopt);

  bool is_evicted_from_back_forward_cache() {
    return is_evicted_from_back_forward_cache_;
  }

  const BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap&
  back_forward_cache_disabled_reasons() const {
    return back_forward_cache_disabled_reasons_;
  }

  bool was_restored_from_back_forward_cache_for_debugging() {
    return was_restored_from_back_forward_cache_for_debugging_;
  }

  // Prevents this frame to do a proactive BrowsingInstance swap (for all
  // navigations on this frame - cross-site and same-site).
  void DisableProactiveBrowsingInstanceSwapForTesting();

  bool HasTestDisabledProactiveBrowsingInstanceSwap() const {
    return has_test_disabled_proactive_browsing_instance_swap_;
  }

  void AddServiceWorkerClient(
      const std::string& uuid,
      base::WeakPtr<ServiceWorkerClient> service_worker_client);
  void RemoveServiceWorkerClient(const std::string& uuid);
  // Returns the last committed ServiceWorkerClient of this frame.
  base::WeakPtr<ServiceWorkerClient> GetLastCommittedServiceWorkerClient();

  // Request a new NavigationClient interface from the renderer and returns the
  // ownership of the mojo::AssociatedRemote. This is intended for use by the
  // NavigationRequest.
  mojo::AssociatedRemote<mojom::NavigationClient>
  GetNavigationClientFromInterfaceProvider();

  // Called to signify the RenderFrameHostImpl that one of its ongoing
  // NavigationRequest's has been cancelled.
  void NavigationRequestCancelled(NavigationRequest* navigation_request,
                                  NavigationDiscardReason reason);

  // Return fenced frames owned by |this|. The returned vector is in the order
  // the fenced frames were added (most recent at end).
  std::vector<FencedFrame*> GetFencedFrames() const;

  // Called when a fenced frame needs to be destroyed.
  void DestroyFencedFrame(FencedFrame& fenced_frame);

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

  // A CommitCallbackInterceptor is used to modify parameters for or cancel a
  // DidCommitNavigation call in tests.
  // WillProcessDidCommitNavigation will be run right after entering a
  // navigation callback and if returning false, will return straight away.
  class CommitCallbackInterceptor {
   public:
    CommitCallbackInterceptor() = default;
    virtual ~CommitCallbackInterceptor() = default;

    virtual bool WillProcessDidCommitNavigation(
        NavigationRequest* navigation_request,
        mojom::DidCommitProvisionalLoadParamsPtr* params,
        mojom::DidCommitProvisionalLoadInterfaceParamsPtr*
            interface_params) = 0;
  };

  // Sets the specified |interceptor|. The caller is responsible for ensuring
  // |interceptor| remains live as long as it is set as the interceptor.
  void SetCommitCallbackInterceptorForTesting(
      CommitCallbackInterceptor* interceptor);

  using UnloadACKCallbackForTesting = base::RepeatingCallback<bool()>;

  // Set a callback to listen to the |OnUnloadACK| for testing.
  void SetUnloadACKCallbackForTesting(
      const UnloadACKCallbackForTesting& callback);

  // Posts a message from a frame in another process to the current renderer.
  void PostMessageEvent(
      const std::optional<blink::RemoteFrameToken>& source_token,
      const std::u16string& source_origin,
      const std::u16string& target_origin,
      blink::TransferableMessage message);

  // Requests to swap the current frame into the frame tree, replacing the
  // `blink::RemoteFrame` it is associated with.
  void SwapIn();

  // Manual RTTI to ensure safe downcasts in tests.
  virtual bool IsTestRenderFrameHost() const;

  using BackForwardCacheDisablingFeatures =
      blink::scheduler::WebSchedulerTrackedFeatures;
  using BackForwardCacheDisablingFeature =
      blink::scheduler::WebSchedulerTrackedFeature;

  // BackForwardCache disabling feature for |this|, used in determining |this|
  // frame's BackForwardCache eligibility. The return value includes all the
  // blocking features from browser, renderer and dedicated workers.
  // See comments at |renderer_reported_bfcache_blocking_details_| and
  // |browser_reported_bfcache_disabling_features_|.
  BackForwardCacheDisablingFeatures GetBackForwardCacheDisablingFeatures()
      const;
  using BackForwardCacheBlockingDetails =
      std::vector<blink::mojom::BlockingDetailsPtr>;
  // Returns a vector of blocking details for |this|. This returns more
  // information than `GetBackForwardCacheDisablingFeatures()`, which returns
  // only a list of features used.
  BackForwardCacheBlockingDetails GetBackForwardCacheBlockingDetails() const;

  // Returns a PrefetchedSignedExchangeCache which is attached to |this|.
  scoped_refptr<PrefetchedSignedExchangeCache>
  EnsurePrefetchedSignedExchangeCache();

  // Clears the entries in the PrefetchedSignedExchangeCache if exists.
  void ClearPrefetchedSignedExchangeCache();

  class BackForwardCacheDisablingFeatureHandle {
   public:
    BackForwardCacheDisablingFeatureHandle();
    BackForwardCacheDisablingFeatureHandle(
        BackForwardCacheDisablingFeatureHandle&&);
    BackForwardCacheDisablingFeatureHandle& operator=(
        BackForwardCacheDisablingFeatureHandle&& other) = default;

    inline ~BackForwardCacheDisablingFeatureHandle() { reset(); }

    // This will reduce the feature count for |feature_| for the first time, and
    // do nothing for further calls.
    inline void reset() {
      if (render_frame_host_) {
        render_frame_host_->OnBackForwardCacheDisablingFeatureRemoved(feature_);
      }
      render_frame_host_ = nullptr;
    }

   private:
    friend class RenderFrameHostImpl;
    BackForwardCacheDisablingFeatureHandle(
        RenderFrameHostImpl* render_frame_host,
        BackForwardCacheDisablingFeature feature);

    base::WeakPtr<RenderFrameHostImpl> render_frame_host_ = nullptr;
    BackForwardCacheDisablingFeature feature_;
  };

  // A feature that blocks back/forward cache is used. This function is used for
  // non sticky blocking features.
  BackForwardCacheDisablingFeatureHandle
  RegisterBackForwardCacheDisablingNonStickyFeature(
      BackForwardCacheDisablingFeature feature);

  // A feature that blocks back/forward cache is used. This function is used for
  // sticky blocking features.
  void OnBackForwardCacheDisablingStickyFeatureUsed(
      BackForwardCacheDisablingFeature feature);

  // Returns true if the frame is frozen.
  bool IsFrozen();

  // Set the `frame_` for sending messages to the renderer process.
  void SetMojomFrameRemote(mojo::PendingAssociatedRemote<mojom::Frame>);

  void GetAudioContextManager(
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver);

  void GetFileSystemManager(
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver);

  void GetGeolocationService(
      mojo::PendingReceiver<blink::mojom::GeolocationService> receiver);

  void GetDeviceInfoService(
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver);

  void GetManagedConfigurationService(
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService>
          receiver);

  void GetFontAccessManager(
      mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver);

  void GetFileSystemAccessManager(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver);

#if !BUILDFLAG(IS_ANDROID)
  void GetHidService(mojo::PendingReceiver<blink::mojom::HidService> receiver);

  void BindSerialService(
      mojo::PendingReceiver<blink::mojom::SerialService> receiver);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  void GetSmartCardService(
      mojo::PendingReceiver<blink::mojom::SmartCardService> receiver);
#endif

  IdleManagerImpl* GetIdleManager();

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

  void CreateBucketManagerHost(
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

  void GetSensorProvider(
      mojo::PendingReceiver<blink::mojom::WebSensorProvider> receiver);

  void CreatePermissionService(
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver);

  void CreatePaymentManager(
      mojo::PendingReceiver<payments::mojom::PaymentManager> receiver);

  void CreatePaymentCredential(
      mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver);

  void GetWebAuthenticationService(
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver);

  void GetVirtualAuthenticatorManager(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver);

  void GetPushMessaging(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver);

  void CreateDedicatedWorkerHostFactory(
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);

  void CreateWebTransportConnector(
      mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver);

  void CreateNotificationService(
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver);

  void CreateInstalledAppProvider(
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);

  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);
  void CreateCodeCacheHostWithKeys(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver,
      const net::NetworkIsolationKey& nik,
      const blink::StorageKey& storage_key);

#if BUILDFLAG(IS_ANDROID)
  void BindNFCReceiver(mojo::PendingReceiver<device::mojom::NFC> receiver);
#endif

  // Binds a `CacheStorage` object for the default bucket.
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

  // For threaded worklets we expose an interface via BrowserInterfaceBrokers to
  // bind `receiver` to a `BlobURLStore` instance, which implements the Blob URL
  // API in the browser process.
  void BindBlobUrlStoreReceiver(
      mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver);

  void BindInputInjectorReceiver(
      mojo::PendingReceiver<mojom::InputInjector> receiver);

  void BindWebOTPServiceReceiver(
      mojo::PendingReceiver<blink::mojom::WebOTPService> receiver);

  void BindDigitalIdentityRequestReceiver(
      mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver);

  void BindFederatedAuthRequestReceiver(
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver);

  void BindRestrictedCookieManager(
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver);
  void BindRestrictedCookieManagerWithOrigin(
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      const net::IsolationInfo& isolation_info,
      const url::Origin& origin,
      net::CookieSettingOverrides cookie_setting_overrides);

  // Requires the following preconditions, reporting a bad message otherwise.
  //
  // 1. This frame's top-frame origin must be potentially trustworthy and
  // have scheme HTTP or HTTPS. (See network::SuitableTrustTokenOrigin's class
  // comment for the rationale.)
  //
  // 2. Private State Tokens must be enabled
  // (network::features::kPrivateStateTokens).
  //
  // 3. This frame's origin must be potentially trustworthy.
  void BindTrustTokenQueryAnswerer(
      mojo::PendingReceiver<network::mojom::TrustTokenQueryAnswerer> receiver);

  // Creates connections to WebUSB interfaces bound to this frame.
  void CreateWebUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

  void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver);

  void BindMediaInterfaceFactoryReceiver(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);

  void BindKeySystemSupportReceiver(
      mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver);

  void BindMediaMetricsProviderReceiver(
      mojo::PendingReceiver<media::mojom::MediaMetricsProvider> receiver);

  void BindVideoEncoderMetricsProviderReceiver(
      mojo::PendingReceiver<media::mojom::VideoEncoderMetricsProvider>
          receiver);

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

  void BindRenderAccessibilityHost(
      mojo::PendingReceiver<blink::mojom::RenderAccessibilityHost> receiver);

  void BindNonAssociatedLocalFrameHost(
      mojo::PendingReceiver<blink::mojom::NonAssociatedLocalFrameHost>
          receiver);

  void CreateOriginTrialStateHost(
      mojo::PendingReceiver<blink::mojom::OriginTrialStateHost> receiver);

  // Prerender2:
  // Tells PrerenderHostRegistry to cancel the prerendering of the page this
  // frame is in, which destroys this frame.
  // Returns true if a prerender was canceled. Does nothing and returns false if
  // `this` is not prerendered.
  bool CancelPrerendering(const PrerenderCancellationReason& reason);
  // Called by MojoBinderPolicyApplier when it receives a kCancel interface
  // while prerendering.
  void CancelPrerenderingByMojoBinderPolicy(const std::string& interface_name);

  // LinkPreview:
  // Called by MojoBinderPolicyApplier when it receives a kCancel interface in
  // preview mode.
  void CancelPreviewByMojoBinderPolicy(const std::string& interface_name);

  // Called when the Activate IPC is sent to the renderer. Puts the
  // MojoPolicyBinderApplier in "loose" mode via PrepareToGrantAll() until
  // DidActivateForPrerending() is called.
  void RendererWillActivateForPrerenderingOrPreview();

  // Prerender2:
  // Called when the Activate IPC is acknowledged by the renderer. Relinquishes
  // the MojoPolicyBinderApplier.
  void RendererDidActivateForPrerendering();

  // https://mikewest.github.io/corpp/#initialize-embedder-policy-for-global
  const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy()
      const {
    return policy_container_host_->cross_origin_embedder_policy();
  }
  CrossOriginEmbedderPolicyReporter* coep_reporter() {
    return coep_reporter_.get();
  }
  void SetCrossOriginOpenerPolicyReporter(
      std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter);

  // Semi-formal definition of COOP:
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
  const network::CrossOriginOpenerPolicy& cross_origin_opener_policy() const {
    return policy_container_host_->cross_origin_opener_policy();
  }

  CrossOriginOpenerPolicyAccessReportManager* coop_access_report_manager() {
    return &coop_access_report_manager_;
  }
  int virtual_browsing_context_group() const {
    return virtual_browsing_context_group_;
  }
  int soap_by_default_virtual_browsing_context_group() const {
    return soap_by_default_virtual_browsing_context_group_;
  }

  const network::mojom::ContentSecurityPolicy* required_csp() {
    return required_csp_.get();
  }

  bool IsCredentialless() const override;

  bool IsLastCrossDocumentNavigationStartedByUser() const override;

  std::vector<base::SafeRef<NavigationHandle>>
  GetPendingCommitCrossDocumentNavigations() const override;

  net::CookieSettingOverrides GetCookieSettingOverrides() override;

  PolicyContainerHost* policy_container_host() const {
    return policy_container_host_.get();
  }

  // This is used by RenderFrameHostManager to ensure the replacement
  // RenderFrameHost is properly initialized when performing an early commit
  // as a recovery for a crashed frame.
  // TODO(crbug.com/40052076): Remove this logic when removing the
  // early commit.
  void SetPolicyContainerForEarlyCommitAfterCrash(
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // A counter which is incremented by one every time `CommitNavigation` or
  // `CommitFailedNavigation` is sent to the renderer.
  int commit_navigation_sent_counter() {
    return commit_navigation_sent_counter_;
  }

  // This function mimics DidCommitProvisionalLoad for page activation
  // (back-forward cache restore or prerender activation).
  void DidCommitPageActivation(NavigationRequest* committing_navigation_request,
                               mojom::DidCommitProvisionalLoadParamsPtr params);

  bool has_unload_handler() const { return has_unload_handler_; }

  // See member declaration for details.
  bool has_committed_any_navigation() const {
    return has_committed_any_navigation_;
  }

  // Whether the document in this frame currently has a navigate event handler
  // registered.
  bool has_navigate_event_handler() const {
    return has_navigate_event_handler_;
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
  // TODO(crbug.com/40615943): Remove this.
  void reset_must_be_replaced() { must_be_replaced_ = false; }

  int renderer_exit_count() const { return renderer_exit_count_; }

  // Re-creates loader factories and pushes them to |RenderFrame|.
  // Used in case we need to add or remove intercepting proxies to the
  // running renderer, or in case of Network Service connection errors.
  void UpdateSubresourceLoaderFactories();

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateCrossOriginPrefetchLoaderFactoryBundle();

  // Returns the BackForwardCacheMetrics associated with the last
  // NavigationEntry this RenderFrameHostImpl committed.
  BackForwardCacheMetrics* GetBackForwardCacheMetrics();

  // Returns a base salt used to generate frame-specific IDs for media-device
  // enumerations.
  const std::string& GetMediaDeviceIDSaltBase() const {
    return media_device_id_salt_base_;
  }

  void set_inner_tree_main_frame_tree_node_id(FrameTreeNodeId id) {
    inner_tree_main_frame_tree_node_id_ = id;
  }
  FrameTreeNodeId inner_tree_main_frame_tree_node_id() const {
    return inner_tree_main_frame_tree_node_id_;
  }

  // These are the content internal equivalents of
  // |RenderFrameHost::ForEachRenderFrameHost| whose comment can be referred to
  // for details. Content internals can also access speculative
  // RenderFrameHostImpls if necessary by using the
  // |ForEachRenderFrameHostIncludingSpeculative| variations.
  void ForEachRenderFrameHostWithAction(
      base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame);
  void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHostImpl*)> on_frame);
  void ForEachRenderFrameHostIncludingSpeculativeWithAction(
      base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame);
  void ForEachRenderFrameHostIncludingSpeculative(
      base::FunctionRef<void(RenderFrameHostImpl*)> on_frame);

  bool DocumentUsedWebOTP() override;

  scoped_refptr<WebAuthRequestSecurityChecker>
  GetWebAuthRequestSecurityChecker();

  base::WeakPtr<RenderFrameHostImpl> GetWeakPtr();
  base::SafeRef<RenderFrameHostImpl> GetSafeRef() const;

  // blink::mojom::LocalFrameHost
  void EnterFullscreen(blink::mojom::FullscreenOptionsPtr options,
                       EnterFullscreenCallback callback) override;
  void ExitFullscreen() override;
  void FullscreenStateChanged(
      bool is_fullscreen,
      blink::mojom::FullscreenOptionsPtr options) override;
  void RegisterProtocolHandler(const std::string& scheme,
                               const GURL& url,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(const std::string& scheme,
                                 const GURL& url,
                                 bool user_gesture) override;
  void DidDisplayInsecureContent() override;
  void DidContainInsecureFormAction() override;
  void MainDocumentElementAvailable(bool uses_temporary_zoom_level) override;
  void SetNeedsOcclusionTracking(bool needs_tracking) override;
  void SetVirtualKeyboardMode(ui::mojom::VirtualKeyboardMode mode) override;
  void VisibilityChanged(blink::mojom::FrameVisibility) override;
  void DidChangeThemeColor(std::optional<SkColor> theme_color) override;
  void DidChangeBackgroundColor(const SkColor4f& background_color,
                                bool color_adjust) override;
  void DidFailLoadWithError(const GURL& url, int32_t error_code) override;
  void DidFocusFrame() override;
  void DidCallFocus() override;
  void EnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy) override;
  void EnforceInsecureNavigationsSet(const std::vector<uint32_t>& set) override;
  void SuddenTerminationDisablerChanged(
      bool present,
      blink::mojom::SuddenTerminationDisablerType disabler_type) override;
  void HadStickyUserActivationBeforeNavigationChanged(bool value) override;
  void ScrollRectToVisibleInParentFrame(
      const gfx::RectF& rect_to_scroll,
      blink::mojom::ScrollIntoViewParamsPtr params) override;
  void BubbleLogicalScrollInParentFrame(
      blink::mojom::ScrollDirection direction,
      ui::ScrollGranularity granularity) override;
  void DidBlockNavigation(
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  void DidChangeLoadProgress(double load_progress) override;
  void DidFinishLoad(const GURL& validated_url) override;
  void DispatchLoad() override;
  void GoToEntryAtOffset(int32_t offset,
                         bool has_user_gesture,
                         std::optional<blink::scheduler::TaskAttributionId>
                             soft_navigation_heuristics_task_id) override;
  void NavigateToNavigationApiKey(
      const std::string& key,
      bool has_user_gesture,
      std::optional<blink::scheduler::TaskAttributionId> task_id) override;
  void NavigateEventHandlerPresenceChanged(bool present) override;
  void UpdateTitle(const std::optional<::std::u16string>& title,
                   base::i18n::TextDirection title_direction) override;
  void UpdateAppTitle(const ::std::u16string& app_title) override;
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override;
  void DidConsumeHistoryUserActivation() override;
  void HandleAccessibilityFindInPageResult(
      blink::mojom::FindInPageResultAXParamsPtr params) override;
  void HandleAccessibilityFindInPageTermination() override;
  void DocumentOnLoadCompleted() override;
  void ForwardResourceTimingToParent(
      blink::mojom::ResourceTimingInfoPtr timing) override;
  void DidDispatchDOMContentLoadedEvent() override;
  void RunModalAlertDialog(const std::u16string& alert_message,
                           bool disable_third_party_subframe_suppresion,
                           RunModalAlertDialogCallback callback) override;
  void RunModalConfirmDialog(const std::u16string& alert_message,
                             bool disable_third_party_subframe_suppresion,
                             RunModalConfirmDialogCallback callback) override;
  void RunModalPromptDialog(const std::u16string& alert_message,
                            const std::u16string& default_value,
                            bool disable_third_party_subframe_suppresion,
                            RunModalPromptDialogCallback callback) override;
  void RunBeforeUnloadConfirm(bool is_reload,
                              RunBeforeUnloadConfirmCallback callback) override;
  void MaybeStartOutermostMainFrameNavigation(
      const std::vector<GURL>& urls) override;
  void UpdateFaviconURL(
      std::vector<blink::mojom::FaviconURLPtr> favicon_urls) override;
  void DownloadURL(blink::mojom::DownloadURLParamsPtr params) override;
  void FocusedElementChanged(bool is_editable_element,
                             bool is_richly_editable_element,
                             const gfx::Rect& bounds_in_frame_widget,
                             blink::mojom::FocusType focus_type) override;
  void TextSelectionChanged(const std::u16string& text,
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
  void CreateNewPopupWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
          blink_popup_widget_host,
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget)
      override;
  void ShowContextMenu(
      mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient>
          context_menu_client,
      const blink::UntrustworthyContextMenuParams& params) override;
  void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& http_method,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination,
      bool include_credentials) override;
  void DidChangeFrameOwnerProperties(
      const blink::FrameToken& child_frame_token,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties) override;
  void DidChangeOpener(
      const std::optional<blink::LocalFrameToken>& opener_frame) override;
  void DidChangeIframeAttributes(
      const blink::FrameToken& child_frame_token,
      blink::mojom::IframeAttributesPtr attributes) override;
  void DidChangeFramePolicy(const blink::FrameToken& child_frame_token,
                            const blink::FramePolicy& frame_policy) override;
  void CapturePaintPreviewOfSubframe(
      const gfx::Rect& clip_rect,
      const base::UnguessableToken& guid) override;
  void SetCloseListener(
      mojo::PendingRemote<blink::mojom::CloseListener> listener) override;
  void Detach() override;
  void DidAddMessageToConsole(
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      uint32_t line_no,
      const std::optional<std::u16string>& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;
  void FrameSizeChanged(const gfx::Size& frame_size) override;
  void DidChangeSrcDoc(const blink::FrameToken& child_frame_token,
                       const std::string& srcdoc_value) override;
  void ReceivedDelegatedCapability(
      blink::mojom::DelegatedCapability delegated_capability) override;
  void SendFencedFrameReportingBeacon(
      const std::string& event_data,
      const std::string& event_type,
      const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
      bool cross_origin_exposed) override;
  void SendFencedFrameReportingBeaconToCustomURL(
      const GURL& destination_url,
      bool cross_origin_exposed) override;
  void SetFencedFrameAutomaticBeaconReportEventData(
      blink::mojom::AutomaticBeaconType event_type,
      const std::string& event_data,
      const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
      bool once,
      bool cross_origin_exposed) override;
  void DisableUntrustedNetworkInFencedFrame(
      DisableUntrustedNetworkInFencedFrameCallback callback) override;
  void ExemptUrlFromNetworkRevocationForTesting(
      const GURL& exempted_url,
      ExemptUrlFromNetworkRevocationForTestingCallback callback) override;
  void SendLegacyTechEvent(
      const std::string& type,
      blink::mojom::LegacyTechEventCodeLocationPtr code_location) override;
  void SendPrivateAggregationRequestsForFencedFrameEvent(
      const std::string& event_type) override;
  void CreateFencedFrame(
      mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
          pending_receiver,
      blink::mojom::RemoteFrameInterfacesFromRendererPtr
          remote_frame_interfaces,
      const blink::RemoteFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token) override;
  void ForwardFencedFrameEventAndUserActivationToEmbedder(
      const std::string& event_type) override;
  void OnViewTransitionOptInChanged(blink::mojom::ViewTransitionSameOriginOptIn
                                        view_transition_opt_in) override;
  void StartDragging(blink::mojom::DragDataPtr drag_data,
                     blink::DragOperationsMask drag_operations_mask,
                     const SkBitmap& unsafe_bitmap,
                     const gfx::Vector2d& cursor_offset_in_dip,
                     const gfx::Rect& drag_obj_rect_in_dip,
                     blink::mojom::DragEventSourceInfoPtr event_info) override;
  void IssueKeepAliveHandle(
      mojo::PendingReceiver<blink::mojom::NavigationStateKeepAliveHandle>
          receiver) override;
  void NotifyStorageAccessed(blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override;
  void RecordWindowProxyUsageMetrics(
      const blink::FrameToken& target_frame_token,
      blink::mojom::WindowProxyAccessType access_type) override;

  // blink::mojom::BackForwardCacheControllerHost:
  void EvictFromBackForwardCache(
      blink::mojom::RendererEvictionReason,
      blink::mojom::ScriptSourceLocationPtr) override;

  void DidChangeBackForwardCacheDisablingFeatures(
      BackForwardCacheBlockingDetails details) override;

  // blink::LocalMainFrameHost overrides:
  void ScaleFactorChanged(float scale) override;
  void ContentsPreferredSizeChanged(const gfx::Size& pref_size) override;
  void TextAutosizerPageInfoChanged(
      blink::mojom::TextAutosizerPageInfoPtr page_info) override;
  void FocusPage() override;
  void TakeFocus(bool reverse) override;
  void UpdateTargetURL(const GURL& url,
                       blink::mojom::LocalMainFrameHost::UpdateTargetURLCallback
                           callback) override;
  void RequestClose() override;
  void ShowCreatedWindow(const blink::LocalFrameToken& opener_frame_token,
                         WindowOpenDisposition disposition,
                         blink::mojom::WindowFeaturesPtr window_features,
                         bool user_gesture,
                         ShowCreatedWindowCallback callback) override;
  void SetWindowRect(const gfx::Rect& bounds,
                     SetWindowRectCallback callback) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidAccessInitialMainDocument() override;
  void Minimize() override;
  void Maximize() override;
  void Restore() override;
  void SetResizable(bool resizable) override;
  void DraggableRegionsChanged(
      std::vector<blink::mojom::DraggableRegionPtr> regions) override;

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
  // content/public/browser/document_user_data.h for more details.
  const DocumentAssociatedData& document_associated_data() const {
    return *document_associated_data_;
  }

  DocumentAssociatedData& document_associated_data() {
    return *document_associated_data_;
  }

  // Called when we commit speculative RFH early due to not having an alive
  // current frame. This happens when the renderer crashes before navigating to
  // a new URL using speculative RenderFrameHost.
  // TODO(crbug.com/40052076): Undo this plumbing after removing the
  // early post-crash CommitPending() call.
  void OnCommittedSpeculativeBeforeNavigationCommit() {
    committed_speculative_rfh_before_navigation_commit_ = true;
  }

  // Returns the child frame if |child_frame_routing_id| is an immediate child
  // of this RenderFrameHost. |child_frame_routing_id| is considered untrusted,
  // so the renderer process is killed if it refers to a RenderFrameHostImpl
  // that is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(int32_t child_frame_routing_id,
                                    bad_message::BadMessageReason reason);

  // Returns the child frame if |child_frame_token| is an immediate child of
  // this RenderFrameHostImpl. |child_frame_token| is considered untrusted, so
  // the renderer process is killed if it refers to a RenderFrameHostImpl that
  // is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(const blink::FrameToken& child_frame_token,
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
  // TODO(crbug.com/40142288): Support unload-in-commit.
  bool ShouldDispatchPagehideAndVisibilitychangeDuringCommit(
      RenderFrameHostImpl* old_frame_host,
      const UrlInfo& dest_url_info);

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateURLLoaderNetworkObserver();

  mojo::PendingRemote<network::mojom::CookieAccessObserver>
  CreateCookieAccessObserver();

  mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
  CreateTrustTokenAccessObserver();

  mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
  CreateSharedDictionaryAccessObserver();

  mojo::PendingRemote<device::mojom::VibrationManagerListener>
  CreateVibrationManagerListener();

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override;

  // network::mojom::TrustTokenAccessObserver:
  void OnTrustTokensAccessed(
      network::mojom::TrustTokenAccessDetailsPtr details) override;

  // network::mojom::SharedDictionaryAccessObserver:
  void OnSharedDictionaryAccessed(
      network::mojom::SharedDictionaryAccessDetailsPtr details) override;

  // device::mojom::VibrationManagerListener:
  void OnVibrate() override;

  void GetSavableResourceLinksFromRenderer();

  // Helper for checking if a navigation to an error page should be excluded
  // from CanAccessDataForOrigin and/or CanCommitOriginAndUrl security checks.
  //
  // It is allowed for |navigation_request| to be null - for example when
  // committing a same-document navigation.
  //
  // The optional |should_commit_unreachable_url| will be set to |true| if the
  // caller should verify that DidCommitProvisionalLoadParams'
  // url_is_unreachable is |true|.
  bool ShouldBypassSecurityChecksForErrorPage(
      NavigationRequest* navigation_request,
      bool* should_commit_error_page = nullptr);

  // Explicitly allow the use of an audio output device in this render frame.
  // When called with a hashed device id string the renderer will be allowed to
  // use the associated device for audio output until this method is called
  // again with a different hashed device id or the origin changes. To remove
  // this permission, this method may be called with the empty string.
  void SetAudioOutputDeviceIdForGlobalMediaControls(
      std::string hashed_device_id);

  // Evicts the document from the BackForwardCache if it is in the cache,
  // and ineligible for caching.
  void MaybeEvictFromBackForwardCache();

  // Returns a filter that should be associated with all AssociatedReceivers for
  // this frame. |interface_name| is used for logging purposes and must be valid
  // for the entire program duration.
  std::unique_ptr<mojo::MessageFilter> CreateMessageFilterForAssociatedReceiver(
      const char* interface_name);

  // TODO(crbug.com/40169570): FrameTree and FrameTreeNode are not const
  // as with prerenderer activation the page needs to move between
  // FrameTreeNodes and FrameTrees. Note that FrameTreeNode can only change for
  // root nodes. As it's hard to make sure that all places handle this
  // transition correctly, MPArch will remove references from this class to
  // FrameTree/FrameTreeNode.
  void SetFrameTreeNode(FrameTreeNode& frame_tree_node);
  void SetFrameTree(FrameTree& frame_tree);

  // RenderFrameHostImpl should not directly reference its FrameTreeNode as
  // associated FrameTreeNode can change during RenderFrameHostImpl's lifetime
  // (crbug.com/1179502). Instead, a dedicated interface (RenderFrameHostOwner)
  // is exposed here.
  // TODO(crbug.com/40169570): Remove RenderFrameHostImpl::SetFrameTreeNode in
  // favour of this method.
  void SetRenderFrameHostOwner(RenderFrameHostOwner* owner) { owner_ = owner; }

  // Builds and return a ClientSecurityState based on the internal
  // RenderFrameHostImpl state. This is never null.
  network::mojom::ClientSecurityStatePtr BuildClientSecurityState() const;
  // For worker script fetches/updates or fetches within workers.
  network::mojom::ClientSecurityStatePtr BuildClientSecurityStateForWorkers()
      const;

  void OnDidRunInsecureContent(const GURL& security_origin,
                               const GURL& target_url);
  void OnDidDisplayContentWithCertificateErrors();
  void OnDidRunContentWithCertificateErrors();

  PeerConnectionTrackerHost& GetPeerConnectionTrackerHost();
  void BindPeerConnectionTrackerHost(
      mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost> receiver);
  void EnableWebRtcEventLogOutput(int lid, int output_period_ms) override;
  void DisableWebRtcEventLogOutput(int lid) override;
  bool IsDocumentOnLoadCompletedInMainFrame() override;
  const std::vector<blink::mojom::FaviconURLPtr>& FaviconURLs() override;

#if BUILDFLAG(ENABLE_MDNS)
  void CreateMdnsResponder(
      mojo::PendingReceiver<network::mojom::MdnsResponder> receiver);
#endif  // BUILDFLAG(ENABLE_MDNS)

  const network::mojom::URLResponseHeadPtr& last_response_head() const {
    return last_response_head_;
  }

  NavigationEarlyHintsManager* early_hints_manager() {
    return early_hints_manager_.get();
  }

  // Create a bundle of subresource factories for an initial empty document.
  // Used for browser-initiated (e.g. no opener) creation of a new main frame.
  // Example scenarios:
  // 1. `window.open` with `rel=noopener` (this path is not used if rel=noopener
  //    is missing;  child frames also don't go through this path).
  // 2. Browser-initiated (e.g. browser-UI-driven) opening of a new tab.
  // 3. Recreating an active main frame when recovering from a renderer crash.
  // 4. Creating a speculative main frame if navigation requires a process swap.
  //
  // Currently the returned bundle is mostly empty - in practice it is
  // sufficient to provide only a NetworkService-bound default factory (i.e. no
  // chrome-extension:// or file:// or data: factories are present today).
  // TODO(lukasza): Revisit the above is necessary.
  //
  // The parameters of the NetworkService-bound default factory (e.g.
  // `request_initiator_origin_lock`, IPAddressSpace, etc.) are associated
  // with the current `last_committed_origin_` (typically an opaque, unique
  // origin for a browser-created initial empty document;  it may be a regular
  // origin when recovering from a renderer crash).
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateSubresourceLoaderFactoriesForInitialEmptyDocument();

  // Prerender2:
  // Dispatches DidFinishLoad and DOMContentLoaded if it occurred pre-activation
  // and was deferred to be dispatched after activation.
  void MaybeDispatchDOMContentLoadedOnPrerenderActivation();
  void MaybeDispatchDidFinishLoadOnPrerenderActivation();

  // Versions of `GetParentOrOuterDocumentOrEmbedder` and
  // `GetOutermostMainFrameOrEmbedder` which exclude embedders which own, but
  // have not yet attached, our frame tree for rendering. These should be
  // avoided in favor of `GetParentOrOuterDocumentOrEmbedder` and
  // `GetOutermostMainFrameOrEmbedder`. This distinction should only matter for
  // cases which implement a view hierarchy such as the implementation of
  // RenderWidgetHostView.
  RenderFrameHostImpl*
  GetParentOrOuterDocumentOrEmbedderExcludingProspectiveOwners() const;
  RenderFrameHostImpl*
  GetOutermostMainFrameOrEmbedderExcludingProspectiveOwners();

  // Computes the nonce to be used for isolation info and storage key.
  // For navigations, populate `fenced_frame_nonce_for_navigation` with
  // `NavigationRequest::ComputeFencedFrameNonce()`.
  std::optional<base::UnguessableToken> ComputeNonce(
      bool is_credentialless,
      std::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation);

  // Return the frame immediately preceding this RenderFrameHost in its parent's
  // children, or nullptr if there is no such node.
  FrameTreeNode* PreviousSibling() const;

  // Return the frame immediately following this RenderFrameHost in its parent's
  // children, or nullptr if there is no such node.
  FrameTreeNode* NextSibling() const;

  // Set the |last_committed_origin_|, |isolation_info_|,
  // |permissions_policy_|, and the RuntimeFeatureStateDocumentData of |this|
  // frame, inheriting both the origin from |creator_frame| as appropriate (e.g.
  // depending on whether |this| frame should be sandboxed / should have an
  // opaque origin instead).
  void SetOriginDependentStateOfNewFrame(RenderFrameHostImpl* creator_frame);

  // Calculates the storage key for this RenderFrameHostImpl using the passed
  // `new_rfh_origin`, and `nonce`, and deriving the storage key's
  // top_level_site` and `ancestor_bit` parameters. If
  // `is_third_party_storage_partitioning_allowed` is provided then that value
  // is used for allowing third-party storage partitioning, if it is
  // std::nullopt then the value is calculated using the top frame's
  // RuntimeFeatureStateReadContext, the enterprise policy, and the
  // base::feature. This also takes into account special embedding cases, such
  // as extensions embedding web iframes, where the top-level frame is not the
  // same as the top-level site for the purposes of calculating the storage key.
  // See the implementation for more details.
  blink::StorageKey CalculateStorageKey(const url::Origin& new_rfh_origin,
                                        const base::UnguessableToken* nonce);

  // Returns the BrowsingContextState associated with this RenderFrameHostImpl.
  // See class comments in BrowsingContextState for a more detailed description.
  const scoped_refptr<BrowsingContextState>& browsing_context_state() const {
    return browsing_context_state_;
  }

  // Returns true if the back/forward cache eviction timer has been started, and
  // false otherwise.
  bool IsBackForwardCacheEvictionTimeRunningForTesting() const;

  // Retrieve proxies in a way that is no longer dependent on access to
  // FrameTreeNode or RenderFrameHostManager.
  RenderFrameProxyHost* GetProxyToParent();

  // Returns the proxy representing `this` to its outer document's SiteInstance.
  // Note that this can only be called for active main RenderFrameHosts. Returns
  // nullptr if `this` is not the main frame of an inner frame tree.
  RenderFrameProxyHost* GetProxyToOuterDelegate();

  void DidChangeReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy);

  float GetPageScaleFactor() const;

#if BUILDFLAG(IS_ANDROID)
  // Perform security checks on Web Authentication requests. These can be
  // called by the Android Java |Authenticator| mojo interface implementation
  // so that they don't have to duplicate security policies.
  // For requests originating from the render process, |effective_origin| will
  // be the same as the last committed origin. However, for request originating
  // from the browser process, this may be different.
  // |is_payment_credential_creation| indicates whether MakeCredential is making
  // a payment credential.
  // |remote_desktop_client_override| optionally contains a
  // RemoteDesktopClientOverride client extension for the request.
  // |PerformGetAssertionWebAuthSecurityChecks| returns a security check result
  // and a boolean representing whether the given origin is cross-origin with
  // any frame in this frame's ancestor chain. This extra cross-origin bit is
  // relevant in case callers need it for crypto signature.
  void PerformGetAssertionWebAuthSecurityChecks(
      const std::string& relying_party_id,
      const url::Origin& effective_origin,
      bool is_payment_credential_get_assertion,
      base::OnceCallback<void(blink::mojom::AuthenticatorStatus, bool)>
          callback);
  void PerformMakeCredentialWebAuthSecurityChecks(
      const std::string& relying_party_id,
      const url::Origin& effective_origin,
      bool is_payment_credential_creation,
      base::OnceCallback<void(blink::mojom::AuthenticatorStatus, bool)>
          callback);
#endif

  using JavaScriptResultAndTypeCallback =
      base::OnceCallback<void(blink::mojom::JavaScriptExecutionResultType,
                              base::Value)>;

  // Runs JavaScript in this frame, without restrictions. ONLY FOR TESTS.
  // This method can optionally:
  // - Trigger a fake user activation notification.
  // - Wait for returned promises to be resolved.
  // - Choose to honor JS content settings.
  void ExecuteJavaScriptForTests(const std::u16string& javascript,
                                 bool has_user_gesture,
                                 bool resolve_promises,
                                 bool honor_js_content_settings,
                                 int32_t world_id,
                                 JavaScriptResultAndTypeCallback callback);

  // Call |HandleAXEvents()| for tests.
  void HandleAXEventsForTests(
      const ui::AXTreeID& tree_id,
      ui::AXUpdatesAndEvents updates_and_events,
      ui::AXLocationAndScrollUpdates location_and_scroll_updates) {
    HandleAXEvents(tree_id, std::move(updates_and_events),
                   std::move(location_and_scroll_updates),
                   *accessibility_reset_token_, {});
  }

  // BucketContext:
  blink::StorageKey GetBucketStorageKey() override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_type) override;
  void BindCacheStorageForBucket(
      const storage::BucketInfo& bucket,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void GetSandboxedFileSystemForBucket(
      const storage::BucketInfo& bucket,
      const std::vector<std::string>& directory_path_components,
      blink::mojom::BucketHost::GetDirectoryCallback callback) override;
  storage::BucketClientInfo GetBucketClientInfo() const override;

  // Returns false if this document not the initial empty document, or if the
  // current document's input stream has been opened with document.open(),
  // causing the document to lose its "initial empty document" status. For more
  // details, see the definition of `FrameTreeNode::is_initial_empty_document_`.
  // This is implemented in the .cc file to avoid a circular dependency on
  // frame_tree_node.h.
  // TODO(crbug.com/41490349): Remove this function. Callers within RFHI
  // should migrate to another way of getting this information such as having a
  // caller pass it or through `owner_`.
  bool is_initial_empty_document() const;

  enum class FencedFrameStatus {
    kNotNestedInFencedFrame,
    kFencedFrameRoot,
    kIframeNestedWithinFencedFrame
  };

  // See comment on the member declaration.
  const base::UnguessableToken& devtools_frame_token() const {
    return devtools_frame_token_;
  }

#if BUILDFLAG(ENABLE_PPAPI)
  RenderFrameHostImplPpapiSupport& GetPpapiSupport();
#endif

  // Returns the sticky bit of the User Activation v2 state of this document.
  bool HasStickyUserActivation() const;
  bool IsActiveUserActivation() const;
  void ClearUserActivation();
  void ConsumeTransientUserActivation();
  void ActivateUserActivation(
      blink::mojom::UserActivationNotificationType notification_type,
      bool sticky_only = false);

  // These are called only when RenderFrameHostOwner is iterating over all
  // frames, not directly from the renderer.
  bool IsHistoryUserActivationActive() const;
  void ConsumeHistoryUserActivation();

  // Sets/resets a temporary UA state used to verify focus transfers that cross
  // a fenced frame boundary.
  void ActivateFocusSourceUserActivation();
  void DeactivateFocusSourceUserActivation();

  // Computes user activation status for history intervention based on the
  // actual sticky activation and
  // `honor_sticky_activation_for_history_intervention_`. Note that this should
  // be invoked on the root RFH and not a subframe's RFH since sticky activation
  // from any frame is sufficient and is aggregated at the root.
  bool HasStickyUserActivationForHistoryIntervention() const;

  // Parameter for `ClosePage()` that indicates whether the request comes from
  // the browser or the renderer. This is used to determine whether navigation
  // should prevent the page from closing, since navigations should not prevent
  // the page from closing if the browser is trying to close the page.
  enum class ClosePageSource { kRenderer, kBrowser };

  // Tells the renderer process to run the page's unload handler.
  // A completion callback is invoked by the renderer when the handler
  // execution completes. The `source` parameter indicates whether this request
  // comes from the browser or the renderer. If the request comes from the
  // renderer, then it may be ignored if a different document commits first.
  void ClosePage(ClosePageSource source);

  // When true, indicates that the unload handlers have already executed (e.g.,
  // after receiving a ClosePage ACK) or that they should be ignored. This is
  // queried while attempting to close the current tab/window.  Should only be
  // used on primary main frames.
  bool IsPageReadyToBeClosed();

  // Retrieves the information about the cookie changes that are observed on the
  // last committed document.
  CookieChangeListener::CookieChangeInfo GetCookieChangeInfo();

  // Records metrics on sudden termination handlers found in this frame and
  // subframes.
  void RecordNavigationSuddenTerminationHandlers();

  // Returns the devtools_navigation_token (see
  // NavigationRequest::devtools_navigation_token()) associated with the last
  // cross-document navigation in this RenderFrameHost. This is the same value
  // that is stored in the DocumentLoader for the RFH's document in the renderer
  // (and therefore it has per-document semantics). Returns std::nullopt for a
  // RenderFrameHost that is the initial empty document (see
  // |is_initial_empty_document_| for definition).
  // Note: This is different from the value returned by GetDevToolsFrameToken(),
  // which is a stable identifier used by DevTools to identify frames and is
  // kept constant across navigations in a frame.
  const std::optional<base::UnguessableToken>& GetDevToolsNavigationToken();

  // Returns if the RenderFrameHostImpl is loaded with the "Cache-Control:
  // no-store" header.
  bool LoadedWithCacheControlNoStoreHeader();

  // Binds the receiver end of the FileBackedBlobFactory interface. The
  // FileBackedBlobFactory implementation follows the lifetime of a document in
  // the browser process and it is responsible for registering file backed blobs
  // capturing the URL from which they are accessed. FileBackedBlobFactory is a
  // navigation-associated interface.
  void BindFileBackedBlobFactory(
      mojo::PendingAssociatedReceiver<blink::mojom::FileBackedBlobFactory>
          receiver);

  // Determine if a focus change coming from the renderer was allowed to happen.
  // This only checks focus calls that crosses a fenced frame boundary. It will
  // badmessage the renderer that made the focus call if it deems the focus
  // should've been disallowed. Returns true is the focus change is allowed,
  // false if it is disallowed.
  bool VerifyFencedFrameFocusChange(RenderFrameHostImpl* focused_rfh);

  // Returns whether moving focus into this RFH will cross a fenced frame
  // boundary. A focus is only considered to cross a boundary if it is moving
  // into a fenced frame, not out of a fenced frame.
  bool TakingFocusWillCrossFencedBoundary(RenderFrameHostImpl* focused_rfh);

  // Record back/forward cache disabling reason. When adding a disabling
  // feature, you should normally call
  // `OnBackForwardCacheDisablingFeatureUsed()` or
  // `OnBackForwardCacheDisablingStickyFeatureUsed()`. Call this only when you
  // don't want eviction to happen.
  void RecordBackForwardCacheDisablingReason(
      BackForwardCacheDisablingFeature feature);

  // Send an automatic beacon of type `event_type` if one was registered
  // with the NavigationRequest's initiator frame using the
  // `window.fence.setReportEventDataForAutomaticBeacons` API.
  void MaybeSendFencedFrameAutomaticReportingBeacon(
      NavigationRequest& navigation_request,
      blink::mojom::AutomaticBeaconType event_type);

  // Determines if this RenderFrameHostImpl is allowed to read from Shared
  // Storage. Only true if this RenderFrameHostImpl is in a fenced frame tree,
  // and if its closest fenced frame root ancestor and all nested fenced frame
  // roots have disabled untrusted network access.
  bool CanReadFromSharedStorage();

  // Returns true if this RFH's compositor should be reused by a speculative
  // RFH with the `speculative_site_instance`.
  // Returns false if the speculative RFH should initialize a new compositor.
  bool ShouldReuseCompositing(
      SiteInstanceImpl& speculative_site_instance) const;

  void NotifyWillCreateRenderWidgetOnCommit();

  // If this RenderFrameHost is a local root (i.e., either the main frame or a
  // subframe in a different process than its parent), this returns the
  // RenderWidgetHost corresponding to this frame. Otherwise this returns null.
  // See also GetRenderWidgetHost(), which walks up the tree to find the nearest
  // local root.
  // Main frame: RenderWidgetHost is owned by the RenderViewHost.
  // Subframe: RenderWidgetHost is owned by this RenderFrameHost.
  RenderWidgetHostImpl* GetLocalRenderWidgetHost() const;

  // Called by `SharedStorageHeaderObserver` to add operation deferral callbacks
  // to `deferred_shared_storage_header_callbacks_` to be run by
  // `RunDeferredSharedStorageHeaderCallbacks()` after commit.
  void AddDeferredSharedStorageHeaderCallback(
      base::OnceCallback<void(NavigationOrDocumentHandle*)> callback);

  // Determine what frames in the frame tree have their network revoked. If this
  // function determines the network has been revoked for a frame, it will mark
  // the relevant FencedFrameProperties as having its network cut off. Network
  // revocation can only happen when network access has been disabled for this
  // fenced frame tree as well as for all of its descendant fenced frame trees.
  void CalculateUntrustedNetworkStatus();

  const base::WeakPtr<PageImpl> auction_initiator_page() const {
    return auction_initiator_page_;
  }

  void set_auction_initiator_page(base::WeakPtr<PageImpl> page_impl) {
    auction_initiator_page_ = page_impl;
  }

  base::Uuid GetBaseAuctionNonce() const { return base_auction_nonce_; }

  void GetBoundInterfacesForTesting(std::vector<std::string>& out);

  void GetBoundAssociatedInterfacesForTesting(std::vector<std::string>& out) {
    associated_registry_->GetInterfacesForTesting(out);
  }

  // Returns the number of accessibility platform node identifiers maintained by
  // this instance's AXNodeIdDelegate implementation.
  size_t GetAxUniqueIdCountForTesting() const { return ax_unique_ids_.size(); }

  // Allows tests to disable the unload event timer to simulate bugs that
  // happen before it fires (to avoid flakiness).
  void DisableUnloadTimerForTesting();

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  // |lifecycle_state_| can either be kSpeculative, kPrerendering, or kActive
  // during RenderFrameHostImpl creation.
  RenderFrameHostImpl(
      SiteInstance* site_instance,
      scoped_refptr<RenderViewHostImpl> render_view_host,
      RenderFrameHostDelegate* delegate,
      FrameTree* frame_tree,
      FrameTreeNode* frame_tree_node,
      int32_t routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      const blink::LocalFrameToken& frame_token,
      const blink::DocumentToken& document_token,
      base::UnguessableToken devtools_frame_token,
      bool renderer_initiated_creation_of_main_frame,
      LifecycleStateImpl lifecycle_state,
      scoped_refptr<BrowsingContextState> browsing_context_state,
      blink::FrameOwnerElementType frame_owner_element_type,
      RenderFrameHostImpl* parent,
      FencedFrameStatus fenced_frame_status);

  // The SendCommit* functions below are wrappers for commit calls
  // made to mojom::NavigationClient.
  // These exist to be overridden in tests to retain mojo callbacks.
  virtual void SendCommitNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          subresource_proxying_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          keep_alive_loader_factory,
      mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
          fetch_later_loader_factory,
      const std::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
      blink::mojom::PolicyContainerPtr policy_container,
      const blink::DocumentToken& document_token,
      const base::UnguessableToken& devtools_navigation_token);
  virtual void SendCommitFailedNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int32_t error_code,
      int32_t extended_error_code,
      const std::optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      const blink::DocumentToken& document_token,
      blink::mojom::PolicyContainerPtr policy_container);

  // The Build*Callback functions below are responsible for building the
  // callbacks for either successful or failed commits.
  // Protected because they need to be called from test overrides.
  mojom::NavigationClient::CommitNavigationCallback
  BuildCommitNavigationCallback(NavigationRequest* navigation_request);
  mojom::NavigationClient::CommitFailedNavigationCallback
  BuildCommitFailedNavigationCallback(NavigationRequest* navigation_request);

  // Protected / virtual so it can be overridden by tests.
  // If `for_legacy` is true, the beforeunload handler is not actually present,
  // nor required to run. In this case the renderer is not notified, but
  // PostTask() is used. PostTask() is used because synchronously proceeding
  // with navigation could lead to reentrancy problems. In particular, there
  // are tests and android WebView using NavigationThrottles to navigate from
  // WillStartRequest(). If PostTask() is not used, then CHECKs would trigger
  // in a NavigationController. See https://crbug.com/365039 for more details.
  virtual void SendBeforeUnload(bool is_reload,
                                base::WeakPtr<RenderFrameHostImpl> impl,
                                bool for_legacy);

 private:
  friend class CommitNavigationPauser;
  friend class RenderFrameHostPermissionsPolicyTest;
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;
  friend class TextInputTestLocalFrame;
  friend class WebContentsSplitCacheBrowserTest;
  friend class RenderFrameHostManagerUnloadBrowserTest;
  friend class NavigationBrowserTest;
  friend class FrameHostInterceptorForPopins;

  FRIEND_TEST_ALL_PREFIXES(NavigatorTest, TwoNavigationsRacingCommit);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           SubframeShowsDialogWhenMainFrameNavigates);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           TimerNotRestartedBySecondDialog);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           ComputeSiteForCookiesParentNavigatedAway);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           CheckIsCurrentBeforeAndAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplTest, NavigationStateKeepAlive);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           FindImmediateLocalRoots);
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
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CloseWithPendingWhileUnresponsive);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CloseWithPendingWhileUnresponsiveWithDevTools);
  FRIEND_TEST_ALL_PREFIXES(
      RenderFrameHostManagerUnloadBrowserTest,
      PendingDeleteRFHProcessShutdownDoesNotRemoveSubframes);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerUnloadBrowserTest,
                           PostMessageToParentWhenSubframeNavigates);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderViewHost);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderWidgetHost);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           BindToWebUIFromWebViaMojo);
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
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           FullscreenAfterFrameUnload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, PagehideHandlerSubframes);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, PagehideHandlerABAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadNestedPendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, PartialPagehideHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           PendingDeletionCheckCompletedOnSubtree);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           DetachedIframeUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionABC);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerSameProcess);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerCrossProcess);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessSSLBrowserTest,
                           PagehideHandlersArePowerful);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessSSLBrowserTest,
                           PagehideHandlersArePowerfulGrandChild);
  FRIEND_TEST_ALL_PREFIXES(
      RenderFrameHostImplTest_NoOriginKeyedProcessesByDefault,
      ExpectedMainWorldOrigin);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplTest,
                           RendererInitiatedCloseIsCancelledIfPageIsntPrimary);
  FRIEND_TEST_ALL_PREFIXES(
      RenderFrameHostImplTest,
      BrowserInitiatedCloseIsNotCancelledIfPageIsntPrimary);
  FRIEND_TEST_ALL_PREFIXES(DocumentUserDataTest, CheckInPendingDeletionState);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest, FrozenAndUnfrozenIPC);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           BlockNameUpdateForBackForwardCache);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           BlockNameUpdateForPendingDelete);
  FRIEND_TEST_ALL_PREFIXES(
      BackForwardCacheBrowserTestWithFlagForAXLocationChange,
      EvictOnAXLocationChangeOrNot);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheBrowsingContextStateBrowserTest,
                           SlowUnloadHandlerInIframe);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           ResetOwnerInPendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           SetOwnerInSpeculativeRFHOwner);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTestWithBFCache,
                           ResetOwnerInBFCache);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTestWithBFCache,
                           ChildFramesHiddenWhileInBFCache);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTestWithBFCache,
                           EvictionInBFCache);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplPrerenderBrowserTest,
                           KeepPrerenderRFHOwnerAfterActivation);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationSuddenTerminationDisablerTypeWithFrameTypeBrowserTest,
      RecordUma);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationSuddenTerminationDisablerTypeBrowserTest,
      NavigationSuddenTerminationDisablerTypeRecordUmaNotHttp);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationSuddenTerminationDisablerTypeBrowserTest,
      NavigationSuddenTerminationDisablerTypeRecordUmaInitialEmptyDocument);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationSuddenTerminationDisablerTypeBrowserTest,
      NavigationSuddenTerminationDisablerTypeRecordUmaSameOrigin);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationSuddenTerminationDisablerTypeBrowserTest,
      NavigationSuddenTerminationDisablerTypeRecordUmaActivation);
  FRIEND_TEST_ALL_PREFIXES(NavigationRequestTest, SharedStorageWritable);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest, SetTitleOnPagehide);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           DetachedIframePagehideHandlerABCB);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheStillLoadingBrowserTest,
                           DoesNotCacheIfFrameStillLoading);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           TestHonorStickyActivationForHistoryIntervention);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           HonorStickyActivationForHistoryInterventionReset);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           TestStickyActivationOnReload);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationControllerHistoryInterventionBrowserTest,
      HonorStickyActivationForHistoryInterventionNotResetOnReplaceState);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           TestHonorStickyActivationCrossDocument);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationControllerHistoryInterventionBrowserTest,
      TestHonorStickyActivationForHistoryInterventionForward);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           TestHonorStickyActivationWithChildFrame);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationControllerHistoryInterventionBrowserTest,
      TestHonorStickyActivationWithChildFrameCrossDocument);
  FRIEND_TEST_ALL_PREFIXES(NavigationPolicyContainerBuilderBrowserTest,
                           FinalPoliciesAboutBlankWithInitiatorAndHistory);

  class SubresourceLoaderFactoriesConfig;

  FrameTreeNode* GetSibling(int relative_offset) const;

  FrameTreeNode* FindAndVerifyChildInternal(
      RenderFrameHostOrProxy child_frame_or_proxy,
      bad_message::BadMessageReason reason);

  // IPC Message handlers.
  void OnForwardResourceTimingToParent(
      const ResourceTimingInfo& resource_timing);
  void OnSetNeedsOcclusionTracking(bool needs_tracking);
  void OnSaveImageFromDataURL(const std::string& url_str);

  // Computes the IsolationInfo for both navigations and subresources.
  //
  // For navigations, |frame_origin| is the origin being navigated to. For
  // subresources, |frame_origin| is the value of |last_committed_origin_|. The
  // boolean `credentialless` specifies whether this resource should be loaded
  // with the restrictions of a credentialless iframe.
  //
  // For navigations, populate `fenced_frame_nonce_for_navigation` with
  // `NavigationRequest::ComputeFencedFrameNonce()`.
  net::IsolationInfo ComputeIsolationInfoInternal(
      const url::Origin& frame_origin,
      net::IsolationInfo::RequestType request_type,
      bool is_credentialless,
      std::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation);

  // Returns whether or not this RenderFrameHost is a descendant of |ancestor|.
  // This is equivalent to check that |ancestor| is reached by iterating on
  // GetParent().
  // This is a strict relationship, a RenderFrameHost is never an ancestor of
  // itself.
  // This does not consider inner frame trees (i.e. not accounting for fenced
  // frames or GuestView).
  bool IsDescendantOfWithinFrameTree(RenderFrameHostImpl* ancestor);

  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       CreateNewWindowCallback callback) override;
  void GetKeepAliveHandleFactory(
      mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver)
      override;
  void DidCommitProvisionalLoad(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params)
      override;
  void CreateChildFrame(
      const blink::LocalFrameToken& frame_token,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          associated_interface_provider_receiver,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const blink::FramePolicy& frame_policy,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
      blink::FrameOwnerElementType owner_type,
      ukm::SourceId document_ukm_source_id) override;
  void StartLoadingForAsyncNavigationApiCommit() override;
  void DidCommitSameDocumentNavigation(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params)
      override;
  void DidOpenDocumentInputStream(const GURL& url) override;
  // |initiator_navigation_state_keep_alive_handle| is needed to ensure that the
  // NavigationStateKeepAlive of the initiator RenderFrameHost can still be
  // retrieved even if the RenderFrameHost has been deleted.
  void BeginNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationStateKeepAliveHandle>
          initiator_navigation_state_keep_alive_handle,
      mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
          renderer_cancellation_listener) override;
  void SubresourceResponseStarted(const url::SchemeHostPort& final_response_url,
                                  net::CertStatus cert_status) override;
  void ResourceLoadComplete(
      blink::mojom::ResourceLoadInfoPtr resource_load_info) override;
  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override;
  void CancelInitialHistoryLoad() override;
  void DidInferColorScheme(
      blink::mojom::PreferredColorScheme color_scheme) override;
  void UpdateEncoding(const std::string& encoding) override;
  void UpdateState(const blink::PageState& state) override;
  void OpenURL(blink::mojom::OpenURLParamsPtr params) override;
  void DidStopLoading() override;

  // blink::mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
          receiver) override;

#if BUILDFLAG(IS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override;
#endif

  friend class RenderAccessibilityHost;
  void HandleAXEvents(
      const ui::AXTreeID& tree_id,
      ui::AXUpdatesAndEvents updates_and_events,
      ui::AXLocationAndScrollUpdates location_and_scroll_updates,
      uint32_t reset_token,
      mojo::ReportBadMessageCallback report_bad_message_callback);
  void HandleAXLocationChanges(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates changes,
      uint32_t reset_token,
      mojo::ReportBadMessageCallback report_bad_message_callback);

  // mojom::DomAutomationControllerHost:
  void DomOperationResponse(const std::string& json_string) override;

  // network::mojom::CookieAccessObserver
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

  // network::mojom::TrustTokenAccessObserver
  void Clone(mojo::PendingReceiver<network::mojom::TrustTokenAccessObserver>
                 observer) override;

  // network::mojom::SharedDictionaryAccessObserver
  void Clone(
      mojo::PendingReceiver<network::mojom::SharedDictionaryAccessObserver>
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
                                        const GURL& url,
                                        bool is_same_document_navigation,
                                        bool is_pdf,
                                        bool is_sandboxed);

  // Returns whether a subframe navigation request should be allowed to commit
  // to the current RenderFrameHost.
  bool CanSubframeCommitOriginAndUrl(NavigationRequest* navigation_request);

  // Asserts that the given RenderFrameHostImpl is part of the same browser
  // context (and crashes if not), then returns whether the given frame is
  // part of the same site instance.
  bool IsSameSiteInstance(RenderFrameHostImpl* other_render_frame_host);

  // Returns whether the current RenderProcessHost has read access to all the
  // files reported in |state|.
  bool CanAccessFilesOfPageState(const blink::PageState& state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |validated_state|.  It is important that the PageState has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromPageState(const blink::PageState& validated_state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |body|.  It is important that the ResourceRequestBody has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromResourceRequestBody(
      const network::ResourceRequestBody& body);

  void UpdatePermissionsForNavigation(NavigationRequest* request);

  // Returns the latest NavigationRequest that has resulted in sending a Commit
  // IPC to the renderer process that hasn't yet been acked by the DidCommit IPC
  // from the renderer process.  Returns null if no such NavigationRequest
  // exists.
  NavigationRequest* FindLatestNavigationRequestThatIsStillCommitting();

  // Creates URLLoaderFactoryParams for main world of |this|, either based on
  // the |navigation_request|, or (if |navigation_request| is null) on the last
  // committed navigation.
  network::mojom::URLLoaderFactoryParamsPtr
  CreateURLLoaderFactoryParamsForMainWorld(
      const SubresourceLoaderFactoriesConfig& config,
      std::string_view debug_tag);

  // Like CreateNetworkServiceDefaultFactoryInternal but also sets up a
  // connection error handler to detect and recover from NetworkService
  // crashes.
  bool CreateNetworkServiceDefaultFactoryAndObserve(
      network::mojom::URLLoaderFactoryParamsPtr params,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);

  // Asks an appropriate `NetworkService` to create a new URLLoaderFactory with
  // the given `params` and binds the factory to the `default_factory_receiver`.
  //
  // The callers typically base the `params` on either 1) the current state of
  // `this` RenderFrameHostImpl (e.g. the origin of the initial empty document,
  // or the last committed origin) or 2) a pending NavigationRequest.
  //
  // If this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.  (This indicates that a layer above //content has
  // wrapped `default_factory_receiver` and may inject arbitrary redirects - for
  // example see WebRequestAPI::MaybeProxyURLLoaderFactory.)
  bool CreateNetworkServiceDefaultFactoryInternal(
      network::mojom::URLLoaderFactoryParamsPtr params,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);

  // Returns true if the ExecuteJavaScript() API can be used on this host.
  // The checks do not apply to ExecuteJavaScriptInIsolatedWorld, nor to
  // ExecuteJavaScriptForTests.  See also AssertFrameWasCommitted method.
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
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> opt_callback,
      blink::mojom::HitTestResponsePtr hit_test_response);

  // Callback that will be called as a response to the call to the method
  // content::mojom::RenderAccessibility::SnapshotAccessibilityTree(). The
  // |callback| passed will be invoked after the renderer has responded with a
  // standalone snapshot of the accessibility tree as |snapshot|.
  void RequestAXTreeSnapshotCallback(AXTreeSnapshotCallback callback,
                                     const ui::AXTreeUpdate& snapshot);

  // Makes a copy of an AXTreeUpdate to send to the destination.
  void CopyAXTreeUpdate(const ui::AXTreeUpdate& snapshot,
                        ui::AXTreeUpdate* snapshot_copy);

  // Callback that will be called as a response to the call to the method
  // blink::mojom::LocalFrame::GetSavableResourceLinks(). The |reply| passed
  // will be a nullptr when the url is not the savable URLs or valid.
  void GetSavableResourceLinksCallback(
      blink::mojom::GetSavableResourceLinksReplyPtr reply);

  // Creates a NavigationRequest for synchronous navigation that have committed
  // in the renderer process. Those are:
  // - same-document renderer-initiated navigations.
  // - synchronous about:blank navigations.
  //
  // TODO(clamy): Eventually, this should only be called for same-document
  // renderer-initiated navigations.
  std::unique_ptr<NavigationRequest>
  CreateNavigationRequestForSynchronousRendererCommit(
      const GURL& url,
      const url::Origin& origin,
      const std::optional<GURL>& initiator_base_url,
      blink::mojom::ReferrerPtr referrer,
      const ui::PageTransition& transition,
      bool should_replace_current_entry,
      bool has_user_gesture,
      const std::vector<GURL>& redirects,
      const GURL& original_request_url,
      bool is_same_document,
      bool is_same_document_history_api_navigation);

  // Helper to process the beforeunload completion callback. |proceed| indicates
  // whether the navigation or tab close should be allowed to proceed.  If
  // |treat_as_final_completion_callback| is true, the frame should stop waiting
  // for any further completion callbacks from subframes. Completion callbacks
  // invoked from the renderer set |treat_as_final_completion_callback| to
  // false, whereas a beforeunload timeout sets it to true. See
  // SendBeforeUnload() for details on `for_legacy`.
  void ProcessBeforeUnloadCompleted(
      bool proceed,
      bool treat_as_final_completion_callback,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time,
      bool for_legacy);

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
  // destroying |frame|. See SendBeforeUnload() for details on `for_legacy`.
  void ProcessBeforeUnloadCompletedFromFrame(
      bool proceed,
      bool treat_as_final_completion_callback,
      RenderFrameHostImpl* frame,
      bool is_frame_being_destroyed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time,
      bool for_legacy);

  // Helper function to check whether the current frame and its subframes need
  // to run beforeunload and, if |send_ipc| is true, send all the necessary
  // IPCs for this frame's subtree. If |send_ipc| is false, this only checks
  // whether beforeunload is needed and returns the answer.  |subframes_only|
  // indicates whether to only check subframes of the current frame, and skip
  // the current frame itself.
  bool CheckOrDispatchBeforeUnloadForSubtree(bool subframes_only,
                                             bool send_ipc,
                                             bool is_reload);

  // |ForEachRenderFrameHost|'s callback used in
  // |CheckOrDispatchBeforeUnloadForSubtree|.
  FrameIterationAction CheckOrDispatchBeforeUnloadForFrame(
      bool subframes_only,
      bool send_ipc,
      bool is_reload,
      bool* found_beforeunload,
      bool* run_beforeunload_for_legacy,
      RenderFrameHostImpl* rfh);

  // Called by |beforeunload_timeout_| when the beforeunload timeout fires.
  void BeforeUnloadTimeout();

  // Update this frame's last committed origin. This will also update the origin
  // and the "has_potentially_trustworthy_unique_origin" bit in the
  // FrameReplicationState.
  void SetLastCommittedOrigin(const url::Origin& origin,
                              bool is_potentially_trustworthy_unique_origin);

  // Stores a snapshot of the inherited base URL from the initiator's
  // FrameLoadRequest, if this document inherited one (e.g., about:srcdoc).
  // TODO(crbug.com/40060678): about:blank frames will also need to inherit base
  // URLs, from the initiator rather than the parent. See
  // https://crbug.com/1356658#c7.
  void SetInheritedBaseUrl(const GURL& inherited_base_url);

  // Called when a navigation commits successfully to |url_info->url|. This
  // will update |last_committed_url_derived_site_info_| with the SiteInfo
  // corresponding to |url_info|.  If |url_info| is empty,
  // |last_committed_url_derived_site_info_| will be cleared.
  //
  // Note that this will recompute the SiteInfo from |url_info| rather than
  // using GetSiteInstance()->GetSiteInfo(), so that
  // |last_committed_url_derived_site_info_| is always meaningful: e.g., without
  // site isolation, b.com could commit in a SiteInstance for a.com, but this
  // function will still compute the last committed SiteInfo as b.com.  For
  // example, this can be used to track which sites have committed in which
  // process.
  void SetLastCommittedSiteInfo(const UrlInfo& url_info);

  // Clears any existing policy and constructs a new policy for this frame,
  // based on its parent frame and the parsed `header_policy`.
  void ResetPermissionsPolicy(
      const blink::ParsedPermissionsPolicy& header_policy);

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
  // roots of A0. Note that this will exclude any speculative RFHs.
  void ForEachImmediateLocalRoot(
      base::FunctionRef<void(RenderFrameHostImpl*)> func_ref);

  // This is the actual implementation of the various overloads of
  // |ForEachRenderFrameHost|.
  void ForEachRenderFrameHostImpl(
      base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame,
      bool include_speculative);

  // Returns the mojom::Frame interface for this frame in the renderer process.
  // May be overridden by friend subclasses for e.g. tests which wish to
  // intercept outgoing messages. May only be called when the RenderFrame in the
  // renderer exists (i.e. RenderFrameCreated() is true).
  mojom::Frame* GetMojomFrameInRenderer();

  // Utility function used to validate potentially harmful parameters sent by
  // the renderer during the commit notification.
  // A return value of true means that the commit should proceed.
  bool ValidateDidCommitParams(NavigationRequest* navigation_request,
                               mojom::DidCommitProvisionalLoadParams* params,
                               bool is_same_document_navigation);
  // Validates whether we can commit |url| and |origin| for a navigation or a
  // document.open() URL update.
  // A return value of true means that the URL & origin can be committed.
  bool ValidateURLAndOrigin(
      const GURL& url,
      const url::Origin& origin,
      bool is_same_document_navigation,
      NavigationRequest* navigation_request,
      std::string origin_calculation_debug_info = std::string());

  // The actual implementation of committing a navigation in the browser
  // process. Called by the DidCommitProvisionalLoad IPC handler.
  void DidCommitNavigation(
      NavigationRequest* committing_navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

  // Updates tracking of potentially isolatable sandboxed iframes, i.e. iframes
  // that could be isolated if features::kIsolateSandboxedIframes is enabled.
  // A frame can only be an out-of-process sandboxed iframe (OOPSIF) if, in
  // addition to the iframe being sandboxed, the url being committed is not
  // about:blank and is same-site to the parent's site (i.e. is not already an
  // OOPIF). Also, the sandbox permissions must not include 'allow-same-origin'.
  // Anytime the commit is a potential OOPSIF, this RenderFrameHostImpl will be
  // tracked in a global list from which we can determine how many potential
  // OOPSIFs exist at any instant in time. Metrics based on the tracked
  // isolatable frames are generated in LogSandboxedIframesIsolationMetrics()
  // when it is called by the metrics recording codepath. Note: sandboxed main
  // frames that have been opened by an OOPSIF are considered isolatable for the
  // purposes of this function, since they could lead to process overhead under
  // a per-origin isolation model. Assumes that `policy_container_host_` is set.
  void UpdateIsolatableSandboxedIframeTracking(
      NavigationRequest* navigation_request);

  // Called when we receive the confirmation that a navigation committed in the
  // renderer. Used by both DidCommitSameDocumentNavigation and
  // DidCommitNavigation.
  // Returns true if the navigation did commit properly, false if the commit
  // state should be restored to its pre-commit value.
  bool DidCommitNavigationInternal(
      std::unique_ptr<NavigationRequest> navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params);

  // Whether or not to reset DocumentAssociatedData at commit. Normally, this
  // data is reset with each cross-document navigation, but there are some
  // exceptions involving speculative RenderFrameHosts.
  bool ShouldResetDocumentAssociatedDataAtCommit() const;

  // Called when we received the confirmation a new document committed in the
  // renderer. It was created from the |navigation|.
  void DidCommitNewDocument(const mojom::DidCommitProvisionalLoadParams& params,
                            NavigationRequest* navigation);

  // Transfers several document properties stored by |navigation_request| into
  // |this|. Helper for DidCommitNewDocument.
  void TakeNewDocumentPropertiesFromNavigation(
      NavigationRequest* navigation_request);

  // Called by the renderer process when it is done processing a same-document
  // commit request.
  void OnSameDocumentCommitProcessed(
      const base::UnguessableToken& navigation_token,
      bool should_replace_current_entry,
      blink::mojom::CommitResult result);

  // Creates URLLoaderFactory objects for |isolated_world_origins|.
  //
  // Properties of the factories (e.g. their client security state) are either
  // based on the |navigation_request|, or (if |navigation_request| is null) on
  // the last committed navigation.
  //
  // TODO(crbug.com/40137011): Remove the method below once Chrome
  // Platform Apps are gone.
  blink::PendingURLLoaderFactoryBundle::OriginMap
  CreateURLLoaderFactoriesForIsolatedWorlds(
      const SubresourceLoaderFactoriesConfig& config,
      const base::flat_set<url::Origin>& isolated_world_origins);

  // Based on the termination |status| and |exit_code|, may generate a crash
  // report to be routed to the Reporting API.
  void MaybeGenerateCrashReport(base::TerminationStatus status, int exit_code);

  // Bitfield values for recording navigation frame-type (main or subframe)
  // combined with whether a sudden termination disabler is present. Currently
  // it only covers unload.
  enum NavigationSuddenTerminationDisablerType : uint32_t {
    kMainFrame = 1 << 0,
    kUnload = 1 << 1,
    kInitialEmptyDocument = 1 << 2,
    kNotHttp = 1 << 3,
    kMaxValue = kNotHttp,
  };
  // Returns information to be recoreded in UMA about sudden termination
  // disablers presence.
  // The return value is ORed values from
  // `NavigationSuddenTerminationDisablerType`.
  // If `same_origin` is true, the tree traversal will only recurse into frames
  // that are the same origin as `this`. E.g. a(b(a)) will not find the second
  // a.
  uint32_t FindSuddenTerminationHandlers(bool same_origin);

  // The reason for calling StartPendingDeletionOnSubtree() / what is causing
  // the RenderFrameHost to enter the "pending deletion" stage.
  enum class PendingDeletionReason {
    // The FrameTreeNode containing the RenderFrameHost is being detached.
    kFrameDetach,
    // The RenderFrameHost is being swapped out for another RenderFrameHost
    // that just committed a navigation.
    kSwappedOut
  };
  // Move every child frame into the pending deletion state.
  // For each process, send the command to delete the local subtree, and wait
  // for unload handlers to finish if needed.
  // This function can be called because the FrameTreeNode containing this
  // RenderFrameHost is getting detached, or if the RenderFrameHost is being
  // unloaded because another RenderFrameHost had committed in the
  // FrameTreeNode.
  void StartPendingDeletionOnSubtree(
      PendingDeletionReason pending_deletion_reason);

  // This function checks whether a pending deletion frame and all of its
  // subframes have completed running unload handlers. If so, this function
  // destroys this frame. This will happen as soon as...
  // 1) The children in other processes have been deleted.
  // 2) The ack (mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame or
  // mojom::FrameHost::Detach) has been
  //    received. It means this frame in the renderer process is gone.
  void PendingDeletionCheckCompleted();

  // Call |PendingDeletionCheckCompleted| recursively on this frame and its
  // children. This is useful for pruning frames with no unload handlers from
  // this frame's subtree.
  void PendingDeletionCheckCompletedOnSubtree();

  // In this RenderFramehost, cancels every:
  // - Non-pending commit NavigationRequest owned by the FrameTreeNode that
  // intends to commit in this RFH
  // - Pending commit NavigationRequest owned by the RenderFrameHost
  // This function should only be called on swapped out RenderFrameHosts.
  void ResetNavigationsUsingSwappedOutRFH();

  // Called when on a subframe that is being deleted when its unload timeout is
  // reached. This immediately deletes the RenderFrameHost.
  void OnSubframeDeletionUnloadTimeout();
  // Called on a frame that is being navigated away from when its unload timeout
  // is reached.
  void OnNavigationUnloadTimeout();

  // Runs interception set up in testing code, if any.
  // Returns true if we should proceed to the Commit callback, false otherwise.
  bool MaybeInterceptCommitCallback(
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params);

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
                                   const url::Origin& origin,
                                   bool is_same_document_navigation,
                                   NavigationRequest* navigation_request,
                                   std::string& origin_calculation_debug_info);
  void LogCannotCommitOriginCrashKeys(const GURL& url,
                                      const url::Origin& origin,
                                      const ProcessLock& process_lock,
                                      bool is_same_document_navigation,
                                      NavigationRequest* navigation_request);

  // Verifies that browser-calculated and renderer-calculated values for some
  // params in DidCommitProvisionalLoadParams match, to ensure we can completely
  // remove the dependence on the renderer-calculated values. Logs crash keys
  // and dumps them without crashing if some params don't match.
  // See crbug.com/1131832.
  void VerifyThatBrowserAndRendererCalculatedDidCommitParamsMatch(
      NavigationRequest* navigation_request,
      const mojom::DidCommitProvisionalLoadParams& params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params);

  // Common handler for displaying a javascript dialog from the Run*Dialog
  // mojo handlers. This method sets up some initial state before asking the
  // delegate to create a dialog.
  void RunJavaScriptDialog(const std::u16string& message,
                           const std::u16string& default_prompt,
                           JavaScriptDialogType dialog_type,
                           bool disable_third_party_subframe_suppresion,
                           JavaScriptDialogCallback callback);

  // Callback function used to handle the dialog being closed. It will reset
  // the state in the associated RenderFrameHostImpl and call the associated
  // callback when done.
  void JavaScriptDialogClosed(JavaScriptDialogCallback response_callback,
                              bool success,
                              const std::u16string& user_input);

  // See |SetIsXrOverlaySetup()|
  bool HasSeenRecentXrOverlaySetup();

  bool has_unload_handlers() const {
    return has_unload_handler_ || has_pagehide_handler_ ||
           has_visibilitychange_handler_ || do_not_delete_for_testing_;
  }

  // Converts a content-internal RenderFrameHostImpl::LifecycleStateImpl into a
  // coarser-grained RenderFrameHost::LifecycleState which can be exposed
  // outside of content.
  static RenderFrameHost::LifecycleState GetLifecycleStateFromImpl(
      LifecycleStateImpl state);

  void BindReportingObserver(
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          reporting_observer_receiver);

  // Check the renderer provided sandbox flags matches with what the browser
  // process computed on its own. This triggers DCHECK and DumpWithoutCrashing()
  //
  // TODO(crbug.com/40051217) Remove this when we are confident the value
  // computed from the browser is always matching.
  void CheckSandboxFlags();

  // Sets the embedding token corresponding to the document in this
  // RenderFrameHost.
  void SetEmbeddingToken(const base::UnguessableToken& embedding_token);

  // Records a DocumentCreated UKM event and the corresponding identifiability
  // study metric. Called when a Document is committed in this frame.
  void RecordDocumentCreatedUkmEvent(
      const url::Origin& origin,
      const ukm::SourceId document_ukm_source_id,
      ukm::UkmRecorder* ukm_recorder,
      bool only_record_identifiability_metric = false);

  // Initializes |policy_container_host_|. Constructor helper.
  //
  // |renderer_initiated_creation_of_main_frame| specifies whether this render
  // frame host's creation was initiated by the renderer process, and this is
  // a main frame. See the constructor for more details.
  void InitializePolicyContainerHost(
      bool renderer_initiated_creation_of_main_frame);

  // Sets |policy_container_host_| and associates it with the current frame.
  // |policy_container_host| must not be nullptr.
  void SetPolicyContainerHost(
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // Initializes |private_network_request_policy_|. Constructor helper.
  void InitializePrivateNetworkRequestPolicy();

  // Returns true if this frame requires a proxy to talk to its parent.
  // Note: Using a proxy to talk to a parent does not imply that the parent
  // is in a different process.
  // (e.g. kProcessSharingWithStrictSiteInstances mode uses proxies for frames
  //  that are in the same process.)
  bool RequiresProxyToParent();

  // Increases by one `commit_navigation_sent_counter_`.
  void IncreaseCommitNavigationCounter();

  // Sets the storage key for the last committed document in this
  // RenderFrameHostImpl.
  void SetStorageKey(const blink::StorageKey& storage_key);

  // Check if we should wait for unload handlers when shutting down the
  // renderer.
  bool ShouldWaitForUnloadHandlers() const;

  // Asserts that `this` is not a speculative frame and calls
  // DumpWithoutCrashing otherwise.  This method should be called from
  // RenderFrameHostImpl's methods that require the caller to "be careful" not
  // to call them on a speculative frame.  One such example is
  // JavaScriptExecuteRequestInIsolatedWorld.
  void AssertFrameWasCommitted() const;

  // Asserts that the BrowserContext (e.g. Profile in the //chrome layer) of
  // this frame hasn't started shutting down.  The owner of the BrowserContext
  // is responsible for closing all WebContents before initiating destruction of
  // the BrowserContext (and closing the WebContents should destroy all the
  // associated RenderFrameHostImpl objects).
  void AssertBrowserContextShutdownHasntStarted();

  // A feature that blocks back/forward cache is used. Count the usage and evict
  // the entry if necessary.
  void OnBackForwardCacheDisablingFeatureUsed(
      BackForwardCacheDisablingFeature feature);

  // A feature that blocks back/forward cache is removed. Update the count of
  // feature usage. This should only be called from
  // |BackForwardCacheDisablingFeatureHandle|.
  void OnBackForwardCacheDisablingFeatureRemoved(
      BackForwardCacheDisablingFeature feature);

  // Create a self-owned receiver that handles incoming BroadcastChannel
  // ConnectToChannel messages from the renderer.
  void CreateBroadcastChannelProvider(
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelProvider>
          receiver);

  // For frames and main thread worklets we use a navigation-associated
  // interface and bind `receiver` to a `BlobURLStore` instance, which
  // implements the Blob URL API in the browser process.
  void BindBlobUrlStoreAssociatedReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::BlobURLStore> receiver);

  TraceProto::LifecycleState LifecycleStateToProto() const;

  perfetto::protos::pbzero::FrameTreeNodeInfo::FrameType GetFrameTypeProto()
      const;

  void BindCacheStorageInternal(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
      const storage::BucketLocator& bucket_locator);

  void DidEnterBackForwardCacheInternal();
  void WillLeaveBackForwardCacheInternal();

  // Returns the BackForwardCacheImpl for the outermost main frame.
  BackForwardCacheImpl& GetBackForwardCache();

  // Return the FrameTreeNode currently owning this RenderFrameHost. In general,
  // we don't want the RenderFrameHost to depend on it, because it might change,
  // or might be missing. An exception is made here during unload. It is invalid
  // to use this function elsewhere.
  // In other cases, the use of the RenderFrameHostOwner interface should be
  // used for communicating with the FrameTreeNode.
  FrameTreeNode* GetFrameTreeNodeForUnload();

  // Close the page ignoring whether it has unload events registered. This is
  // called either (1) when the unload events have already run in the renderer
  // and the ACK is received, or (2) when a timeout for running those events
  // has expired. The `source` parameter indicates whether this request comes
  // from the browser or the renderer. If the request comes from the renderer,
  // then it may be ignored if a different document commits first.
  void ClosePageIgnoringUnloadEvents(ClosePageSource source);

  // Called when this frame's page has started closing via ClosePage(), and the
  // timer for running unload events has expired. The `source` parameter
  // indicates whether this request comes from the browser or the renderer. If
  // the request comes from the renderer, then it may be ignored if a different
  // document commits first.
  void ClosePageTimeout(ClosePageSource source);

  // Perform pre-conditions checks on RenderFrameHost lifecycle state and fenced
  // frame properties that are common for `SendFencedFrameReportingBeacon()` and
  // `SendFencedFrameReportingBeaconToCustomURL()`, both are from renderers. If
  // checks fail, it implies none of the reporting beacons can be sent. This
  // function should only handle checks not specific to individual destination
  // and event data.
  // Note: This function has side effects. It may terminate misbehaving
  // renderers. It may also add messages for certain cases that return false.
  bool IsFencedFrameReportingFromRendererAllowed(bool cross_origin_exposed);

  // Helper function that handles creating and sending a fenced frame beacon for
  // a given destination.
  void SendFencedFrameReportingBeaconInternal(
      const FencedFrameReporter::DestinationVariant& event_variant,
      blink::FencedFrame::ReportingDestination destination,
      std::optional<int64_t> navigation_id = std::nullopt);

  // Indicates whether this frame has third-party storage
  // partitioning enabled. This depends on the deprecation trial (which can
  // block), content browser client (which can block), and base feature (which
  // can allow/block). The `new_rfh_origin` is the same one passed into
  // CalculateStorageKey.
  bool IsThirdPartyStoragePartitioningEnabled(
      const url::Origin& new_rfh_origin);

  // Returns the recursive list of parent frames starting with `this` and ending
  // with the top-level frame, or one frame before the top-level frame if the
  // top-level frame is an extension. The `new_rfh_origin` is the same one
  // passed into CalculateStorageKey.
  std::vector<RenderFrameHostImpl*> GetAncestorChainForStorageKeyCalculation(
      const url::Origin& new_rfh_origin);

  // Returns whether the `RenderFrameHost` can use Additional Windowing Controls
  // APIs.
  // https://github.com/ivansandrk/additional-windowing-controls/blob/main/awc-explainer.md
  bool CanUseWindowingControls(std::string_view js_api_name);

  // Notifies when the renderer side Widget instance has been created and mojo
  // interfaces to it can be bound.
  void RendererWidgetCreated();

  // Returns an `UrgentMessageScope` if the feature to prioritize navigation
  // IPCs is enabled and this is a main frame on a visible page.
  std::optional<mojo::UrgentMessageScope> MakeUrgentMessageScopeIfNeeded();

#if BUILDFLAG(IS_ANDROID)
  // These functions are called after a WebAuthn relying party check has
  // completed. See `PerformMakeCredentialWebAuthSecurityChecks` and
  // `PerformGetAssertionWebAuthSecurityChecks`.
  void OnGetAssertionWebAuthSecurityChecksCompleted(
      base::OnceCallback<void(blink::mojom::AuthenticatorStatus, bool)>
          callback,
      bool is_cross_origin,
      blink::mojom::AuthenticatorStatus status);
  void OnMakeCredentialWebAuthSecurityChecksCompleted(
      base::OnceCallback<void(blink::mojom::AuthenticatorStatus, bool)>
          callback,
      bool is_cross_origin,
      blink::mojom::AuthenticatorStatus status);
#endif

  // Notifies the RenderProcessHost instance that this frame no longer has any
  // media stream. Called when this render frame is deleted or when the process
  // is gone.
  void CleanUpMediaStreams();

  // Mark network as having been disabled for `nonce` in the active fenced frame
  // properties once it was disabled in the network service, and resolve the
  // `callback` which came from the window.fence.disableUntrustedNetwork call.
  void RevokeNetworkForNonceCallback(
      base::UnguessableToken nonce,
      DisableUntrustedNetworkInFencedFrameCallback callback);

  // Notifies the RenderProcessHost instance that this frame started to commit
  // navigation. This signal is used to increase the priority of the renderer
  // process (see: crbug/351953350).
  void BoostRenderProcessForLoading();
  // Notifies the RenderProcessHost instance that this frame does not require
  // increasing the priority of the renderer process anymore.
  void MaybeResetBoostRenderProcessForLoading();

  // A discard optimization that attempts a shutdown of the associated render
  // process. Shutdown may be reattempted if unsuccessful to give outstanding
  // keep-alive requests a chance to resolve before timing out.
  void CleanupRenderProcessForDiscardIfPossible();

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

  const raw_ptr<RenderFrameHostDelegate> delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  const scoped_refptr<SiteInstanceImpl> site_instance_;

  // The agent scheduling group this RenderFrameHost is associated with. It is
  // initialized through a call to
  // site_instance_->GetOrCreateAgentSchedulingGroupHost() at RenderFrameHost
  // creation time.
  // This ref is used to avoid recreating the renderer process if it has
  // crashed, since using
  // SiteInstance::GetProcess()/GetOrCreateAgentSchedulingGroupHost() has the
  // side effect of creating the process again if it is gone.
  //
  // It is a `SafeRef` so that the browser process crashes cleanly if `this`
  // unintentionally outlives its associated `RenderFrameProcessHost` but tries
  // to access it or its associated `AgentSchedulingGroupHost` (see
  // crbug.com/1297030).
  const base::SafeRef<AgentSchedulingGroupHost> agent_scheduling_group_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  raw_ptr<FrameTree> frame_tree_ = nullptr;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  // TODO(crbug.com/40169570): Remove this after clearing directly all
  // references of FrameTreeNode in RenderFrameHost. Please refer to the
  // description of the new `owner_` field.
  raw_ptr<FrameTreeNode> frame_tree_node_ = nullptr;

  // Interface for RenderFrameHost to communicate with FrameTreeNode owning it,
  // which can be null or can change during the lifetime of RenderFrameHost.
  // This communication is intentionally restricted to minimise the chance of
  // mistakes when the associated FrameTreeNode changes (e.g. during prerender
  // activations).
  //
  // All RenderFrameHosts are created with a non-null `owner_`. When
  // RenderFrameHostManager replaces its current RenderFrameHost with another
  // RenderFrameHost (e.g., after a navigation), the old RenderFrameHost's
  // `owner_` becomes null. The old RenderFrameHost will then enter back-forward
  // cache or become pending deletion.
  //
  // Invariants:
  // - If `lifecycle_state_` is one of: kSpeculative, kPrerendering, kActive, or
  //   kPendingCommit then `owner_` != nullptr.
  //
  // - If `lifecycle_state_` == kBackForwardCache, then the top-level
  //   RenderFrameHost has no `owner_`. RenderFrameHosts nested below in iframes
  //   still have an `owner_` while in the back-forward cache.
  //
  // - If `lifecycle_state_` is kRunningUnloadHandlers or kReadyToBeDeleted
  //   (i.e., pending deletion), then `owner_` will sometimes be null.
  //   Specifically, RenderFrameHosts that have been replaced (e.g., after a
  //   navigation) will have a null `owner_`, while their children will continue
  //   to have a non-null `owner_`. Detached <iframe> RenderFrameHosts also
  //   continue to have a non-null `owner_`.
  //
  // In particular:
  // - IsActive() => owner_.
  // - !owner_    => IsPendingDeletion() || IsInBackForwardCache().
  raw_ptr<RenderFrameHostOwner> owner_ = nullptr;

  // Stores all of the state related to each browsing context +
  // BrowsingInstance. This includes proxy hosts, and replication state, and
  // will help facilitate the full removal of references to frame_tree_ and
  // frame_tree_node_ (per crbug.com/1179502).
  // TODO(crbug.com/40205442): make this field const when legacy mode is
  // removed.
  scoped_refptr<BrowsingContextState> browsing_context_state_;

  // Enum for the type of the frame owner element for a frame.
  blink::FrameOwnerElementType frame_owner_element_type_;

  // The immediate children of this specific frame.
  std::vector<std::unique_ptr<FrameTreeNode>> children_;

  // The active parent RenderFrameHost for this frame, if it is a subframe.
  // Null for the main frame.  This is cached because the parent FrameTreeNode
  // may change its current RenderFrameHost while this child is pending
  // deletion, and GetParent() should never return a different value, even if
  // this RenderFrameHost is on the pending deletion list and the parent
  // FrameTreeNode has changed its current RenderFrameHost.
  const raw_ptr<RenderFrameHostImpl> parent_ = nullptr;

  // Number of times we need to iterate from a RenderFrameHost to its parent
  // until we reach main RenderFrameHost (i.e. one which doesn't have a parent).
  // Note that that means this value is scoped to a given FrameTree and the
  // cases when a FrameTree embeds another FrameTree are not reflected here.
  const unsigned int depth_ = 0u;

  // The FrameNavigationEntry for the current document in this RenderFrameHost,
  // shared with any NavigationEntries that reference it. Updated after every
  // commit.
  // For now, there are cases when this can be null (e.g., initial state, or
  // cases from https://crbug.com/608402 when the FrameNavigationEntry is
  // missing for a frame).
  // TODO(crbug.com/40217743): Ensure this is always set, and use it to
  // avoid separately storing the last committed URL and origin here.
  scoped_refptr<FrameNavigationEntry> last_committed_frame_entry_;

  // Tracks this frame's last committed navigation's URL. Note that this will be
  // empty before the first commit in this *RenderFrameHost*, even if the
  // FrameTreeNode has committed before with a different RenderFrameHost.
  // The URL is always set to the "commit URL" (the URL set in
  // DidCommitProvisionalLoadParams) after every committed navigation, and also
  // when the renderer process crashes (where it's reset to empty).
  // Note that the value tracked here might be different than the value for
  // `last_document_url` in RendererURLInfo, which tracks the last "document
  // URL" which might not necessarily come from a committed navigation (e.g.
  // the URL can change due to document.open()) or the same as the URL used in
  // DidCommitProvisionalLoadParams (e.g. loadDataWithBaseURL which uses the
  // "base URL" as the "document URL" but the data URL as the "committed URL").
  GURL last_committed_url_;

  // See comment in the definition of RendererURLInfo.
  RendererURLInfo renderer_url_info_;

  // Track this frame's last committed origin.
  url::Origin last_committed_origin_;

  // Tracks the id of the navigation that created the document. It is updated
  // after receiving DidCommitNavigation IPC. Same-document navigation are not
  // updating this field. It will be empty `0` for the initial empty document.
  int64_t navigation_id_ = 0;

  // For about:blank and about:srcdoc documents, this tracks the inherited base
  // URL, snapshotted from the initiator's FrameLoadRequest. This is an empty
  // URL for all other cases.
  GURL inherited_base_url_;

  // The storage key for the last committed document in this
  // RenderFrameHostImpl.
  blink::StorageKey storage_key_;

  // The policy to apply to private network requests for subresources issued by
  // the last committed document. Set to a default value until a document
  // commits for the first time. The default value depends on whether certain
  // feature flags are enabled, see |DerivePrivateNetworkRequestPolicy()|.
  //
  // This property normally depends on the last committed origin and the state
  // of |ContentBrowserClient| at the time the navigation committed. Due to the
  // fact that this is based on the origin computed by the browser process in
  // |NavigationRequest|, whereas |last_commited_origin_| is computed by the
  // renderer process (see crbug.com/888079), there can be rare discrepancies.
  //
  // TODO(crbug.com/40092527): Simplify the above comment when the
  // behavior it explains is fixed.
  network::mojom::PrivateNetworkRequestPolicy private_network_request_policy_ =
      network::mojom::PrivateNetworkRequestPolicy::kBlock;

  // Track the SiteInfo of the last site we committed successfully, as obtained
  // from SiteInfo::CreateInternal() called on the last committed UrlInfo.
  // Note that this might be different from this frame's SiteInfo (i.e.,
  // GetSiteInstance()->GetSiteInfo()) on platforms with no site isolation.
  // This is used for tracking which sites have committed in various renderer
  // processes to support process reuse policies.
  // TODO(crbug.com/40176090): Remove this once SiteInstanceGroup is
  // fully implemented, as at that point the SiteInstance's SiteInfo will be the
  // same as the URL-derived SiteInfo.
  SiteInfo last_committed_url_derived_site_info_;

  // The most recent non-error URL to commit in this frame.
  // TODO(clamy): Remove this in favor of GetLastCommittedURL().
  // See https://crbug.com/588314.
  GURL last_successful_url_;

  // The http method of the last committed navigation.
  std::string last_http_method_ = "GET";

  // The http status code of the last committed navigation.
  int last_http_status_code_ = 0;

  // The POST ID of the last committed navigation.
  int64_t last_post_id_ = 0;

  // Whether the last committed navigation's CommonNavigationParams'
  // `has_user_gesture` is true or not. Note that this doesn't always represent
  // whether the navigation was started with user activation or not, because the
  // user activation can be filtered out at a later point to avoid being exposed
  // to the committed document. Also, this is just the cached value of what
  // happened during the last navigation, and does not reflect the user
  // activation state of this RenderFrameHost. To get the current/live user
  // activation state, get the value from RenderFrameHostImpl's
  // HasStickyUserActivation() or HasTransientUserActivation() instead.
  bool last_committed_common_params_has_user_gesture_ = false;

  // Whether the last cross-document committed navigation was initiated from the
  // browser (e.g. typing on the location bar) or from the renderer while having
  // transient user activation
  bool last_cross_document_navigation_started_by_user_ = false;

  // Whether the last committed navigation is to an error document.
  bool is_error_document_ = false;

  // Local root subframes directly own their RenderWidgetHost.
  // Please see comments about the GetLocalRenderWidgetHost() function.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  std::unique_ptr<RenderWidgetHostImpl> owned_render_widget_host_;

  const int routing_id_;

  // Boolean indicating whether this RenderFrameHost is being actively used or
  // is waiting for mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame and
  // thus pending deletion.
  bool is_waiting_for_unload_ack_ = false;

  // Tracks the creation state of the RenderFrame in renderer process for this
  // RenderFrameHost.
  enum class RenderFrameState {
    // A RenderFrame has never been created for this RenderFrameHost. The next
    // state will be kCreated or kDeleted.
    kNeverCreated = 0,
    // A RenderFrame has been created in the renderer and is still in that
    // state. The next state will be kDeleting.
    kCreated,
    // A RenderFrame has either
    // - been cleanly deleted
    // - its renderer process has exited or crashed
    // We will call observers of RenderFrameDeleted in this state and this
    // allows us to CHECK if an observer causes us to attempt to change state
    // during deletion. See https://crbug.com/1146573. The next state will
    // be kDeleted and we will move to that before exiting RenderFrameDeleted.
    kDeleting,
    // A RenderFrame has either
    // - been cleanly deleted
    // - its renderer process has exited or crashed
    // The next state may be kCreated if the RenderFrameHost is being reused
    // after a crash or the RenderFrameHost may be destroyed.
    kDeleted,
  };
  RenderFrameState render_frame_state_ = RenderFrameState::kNeverCreated;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Set to true when there is a pending FrameMsg_BeforeUnload message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_completion_ = false;

  // Valid only when |is_waiting_for_beforeunload_completion_| is true. This
  // indicates whether a subsequent request to launch a modal dialog should be
  // honored or whether it should implicitly cause the unload to be canceled.
  bool beforeunload_dialog_request_cancels_unload_ = false;

  // Valid only when is_waiting_for_beforeunload_completion_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true).
  bool unload_ack_is_for_navigation_ = false;

  // The timeout monitor that runs from when the beforeunload is started in
  // DispatchBeforeUnload() until either the render process invokes the
  // respective completion callback (ProcessBeforeUnloadCompleted()), or until
  // the timeout triggers.
  std::unique_ptr<input::TimeoutMonitor> beforeunload_timeout_;

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
  std::set<raw_ptr<RenderFrameHostImpl, SetExperimental>>
      beforeunload_pending_replies_;

  // During beforeunload, keeps track whether a dialog has already been shown.
  // Used to enforce at most one dialog per navigation.  This is tracked on the
  // frame that is being navigated, and not on any of its subframes that might
  // have triggered a dialog.
  bool has_shown_beforeunload_dialog_ = false;

  // Tracks the process of closing the current page.  Only used on primary main
  // frames.
  enum class PageCloseState {
    kNotClosing,

    // Set to true when waiting for a blink.mojom.LocalMainFrame.ClosePage()
    // to complete.
    kRunningUnloadHandlers,

    // Set to true when renderer-side unload handlers have run (or timed out)
    // and the current page is ready to be closed.
    kReadyToBeClosed
  };
  PageCloseState page_close_state_ = PageCloseState::kNotClosing;

  // The timeout monitor that runs from when the page close is started in
  // ClosePage() until either the renderer process ACKs the close, or until the
  // timeout triggers and the page is forcibly closed.
  std::unique_ptr<input::TimeoutMonitor> close_timeout_;

  // Returns whether the tab was previously discarded.
  // This is passed to CommitNavigationParams in NavigationRequest.
  bool was_discarded_ = false;

  // Indicates whether this RenderFrameHost is in the process of loading a
  // document or not, and if so, whether it should show loading UI.
  LoadingState loading_state_ = LoadingState::NONE;

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.  Tracking this
  // allows us to send things like title and state updates to the latest
  // relevant NavigationEntry.
  int nav_entry_id_ = 0;

  // Used to clean up this RFH when the unload event is taking too long to
  // execute. May be null in tests.
  std::unique_ptr<input::TimeoutMonitor> unload_event_monitor_timeout_;

  // GeolocationService which provides Geolocation.
  std::unique_ptr<GeolocationServiceImpl> geolocation_service_;

  // IdleManager which provides Idle status.
  std::unique_ptr<IdleManagerImpl> idle_manager_;

  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_registry_;

  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces_;

  // The object managing the accessibility tree for this frame.
  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager_;

  // A mapping of each AXNodeID managed by `browser_accessibility_manager_`,
  // which is only unique within its renderer, to an AXUniqueId, which is unique
  // within the scope of the web contents.
  std::map<ui::AXNodeID, ui::AXUniqueId> ax_unique_ids_;

  // This is the value of the reset token expected for accessibility messages.
  // Any message with a different reset token will be dropped.
  // std::nullopt means that accessibility has never been turned on for
  // this renderer (delegate_->GetAccessibilityMode().is_mode_off()).
  std::optional<uint32_t> accessibility_reset_token_;

  // The instant just before a set of accessibility mode flags are sent to the
  // renderer. This is used at the starting point when measuring the time to
  // receive and process a response from the renderer. When
  // `is_first_accessibility_request_` is true, this includes the time to bind
  // to the renderer's `RenderAccessibility` interface.
  base::TimeTicks accessibility_reset_start_;

  // True when the first set of accessibility mode flags are sent to the
  // renderer. This is used to split timing-related accessibility metrics into
  // two groups: one for the initial request when accessibility is first enabled
  // for a WebContents' frames, and one for all subsequent requests.
  bool is_first_accessibility_request_ = true;

  // The last AXTreeData for this frame received from the RenderFrame.
  ui::AXTreeData ax_tree_data_;

  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
#if BUILDFLAG(IS_ANDROID)
  base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>
      smart_clip_callbacks_;
#endif  // BUILDFLAG(IS_ANDROID)

  // Callback when an event is received, for testing.
  AccessibilityCallbackForTesting accessibility_testing_callback_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_ = false;

  // Remotes must be reset in TearDownMojoConnection().
  // Holder of Mojo connection with ImageDownloader service in Blink.
  mojo::Remote<blink::mojom::ImageDownloader> mojo_image_downloader_;

  // Holder of Mojo connection with FindInPage service in Blink.
  mojo::AssociatedRemote<blink::mojom::FindInPage> find_in_page_;

  // Holder of Mojo connection with the LocalFrame in Blink.
  mojo::AssociatedRemote<blink::mojom::LocalFrame> local_frame_;

  // Holder of Mojo connection with the LocalMainFrame in Blink. This
  // remote will be valid when the frame is the active main frame.
  mojo::AssociatedRemote<blink::mojom::LocalMainFrame> local_main_frame_;

  // Holds the cross-document NavigationRequests that are waiting to commit.
  // These are navigations that have passed ReadyToCommit stage and are waiting
  // for a matching commit IPC.
  std::map<NavigationRequest*, std::unique_ptr<NavigationRequest>>
      navigation_requests_;

  // Holds same-document NavigationRequests while waiting for the navigations
  // to commit.
  // TODO(crbug.com/40150899): Use the NavigationRequest as key once
  // NavigationRequests are bound to same-document DidCommit callbacks,
  // similar to |navigation_requests_| above.
  base::flat_map<base::UnguessableToken, std::unique_ptr<NavigationRequest>>
      same_document_navigation_requests_;

  // The associated WebUIImpl and its type. They will be set if the current
  // document is from WebUI source. Otherwise they will be null and
  // WebUI::kNoWebUI, respectively.
  std::unique_ptr<WebUIImpl> web_ui_;
  WebUI::TypeID web_ui_type_ = WebUI::kNoWebUI;

  // If true, then the RenderFrame has selected text.
  bool has_selection_ = false;

  // Indicates the number of media streams (audio or video) that are tracked
  // via OnMediaStreamAdded/OnMediaStreamRemoved, split by the stream type.
  std::array<int, MediaStreamType::kCount> media_stream_counts_ = {};

  // If true, then this RenderFrameHost is waiting to update its
  // LifecycleStateImpl. Happens when the old RenderFrameHost is waiting to
  // either enter BackForwardCache or PendingDeletion. In this case, the old
  // RenderFrameHost's lifecycle state remains in kActive. During this period,
  // the RenderFrameHost is no longer the current one. The flag is again
  // updated once the lifecycle state changes.
  //
  // TODO(crbug.com/40168690): Remove this bool and refactor
  // RenderFrameHostManager::CommitPending() to avoid having a time window where
  // we don't know what the old RenderFrameHost's next lifecycle state should
  // be.
  bool has_pending_lifecycle_state_update_ = false;
  // When `has_pending_lifecycle_state_update_` is true and this RenderFrameHost
  // is not a subframe, this is the type of frame from which this
  // RenderFrameHost was just unset. This is used by `IsInPrimaryMainFrame` as
  // it needs to know what the frame type was during this pending update window
  // when we don't have an `owner_`.
  // TODO(crbug.com/40168690): This can be removed alongside
  // `has_pending_lifecycle_state_update_`.
  std::optional<FrameType> last_main_frame_type_pending_lifecycle_update_;

  // Used for tracking the latest size of the RenderFrame.
  std::optional<gfx::Size> frame_size_;

  // This boolean indicates whether the RenderFrame has committed *any*
  // navigation or not. Starts off false and is set to true for the lifetime of
  // the RenderFrame when the first CommitNavigation message is sent to the
  // RenderFrame. It is reset after a renderer process crash.
  //
  // Note that some use cases may still consider a RenderFrameHost used after a
  // crash, in which case is_initial_empty_document() may be a more relevant
  // check. Also, unlike is_initial_empty_document(), this field does not change
  // if this frame is modified via document.open().  However, note that
  // is_initial_empty_document() is updated at DidCommitNavigation() time,
  // whereas this field is updated at CommitNavigation() time.
  bool has_committed_any_navigation_ = false;

  bool must_be_replaced_ = false;

  // Counts the number of times the associated renderer process has exited.
  // This is used to track problematic RenderFrameHost reuse.
  // TODO(crbug.com/40166847): Remove once enough data has been
  // collected.
  int renderer_exit_count_ = 0;

  // Receivers must be reset in TearDownMojoConnection().
  mojo::AssociatedReceiver<mojom::FrameHost> frame_host_associated_receiver_{
      this};
  mojo::AssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
      associated_interface_provider_receiver_{this};
  mojo::AssociatedReceiver<blink::mojom::BackForwardCacheControllerHost>
      back_forward_cache_controller_host_associated_receiver_{this};
  mojo::AssociatedRemote<mojom::Frame> frame_;
  mojo::AssociatedRemote<mojom::FrameBindingsControl> frame_bindings_control_;
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>
      local_frame_host_receiver_{this};
  mojo::Receiver<blink::mojom::NonAssociatedLocalFrameHost>
      non_associated_local_frame_host_receiver_{this};
  // Should only be bound when the frame is a swapped in main frame.
  mojo::AssociatedReceiver<blink::mojom::LocalMainFrameHost>
      local_main_frame_host_receiver_{this};

  // If this is true then this main-frame object was created in response to a
  // renderer initiated request. Init() will be called when the renderer wants
  // the frame to become visible and to perform navigation requests. Until then
  // navigation requests should be queued.
  bool waiting_for_init_;

  // If true then this frame's document has a focused element which is editable.
  bool has_focused_editable_element_ = false;

  // If true then this frame's document has a focused element which is richly
  // editable.
  bool has_focused_richly_editable_element_ = false;

  std::unique_ptr<PendingNavigation> pending_navigate_;

  // Renderer-side states that blocks fast shutdown of the frame.
  bool has_before_unload_handler_ = false;
  bool has_unload_handler_ = false;
  bool has_pagehide_handler_ = false;
  bool has_visibilitychange_handler_ = false;

  // Tracks whether any navigate event handlers for the Navigation API are
  // registered in the current document. This is useful for tracking whether
  // the document might be allowed to cancel certain history navigations.
  bool has_navigate_event_handler_ = false;

  std::optional<RenderFrameAudioOutputStreamFactory>
      audio_service_audio_output_stream_factory_;
  std::optional<RenderFrameAudioInputStreamFactory>
      audio_service_audio_input_stream_factory_;

  // Hosts blink::mojom::PresentationService for the RenderFrame.
  std::unique_ptr<PresentationServiceImpl> presentation_service_;

  // Hosts blink::mojom::FileSystemManager for the RenderFrame.
  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_;

  // Hosts blink::mojom::PushMessaging for the RenderFrame.
  std::unique_ptr<PushMessagingManager> push_messaging_manager_;

  // Hosts blink::mojom::SpeechSynthesis for the RenderFrame.
  std::unique_ptr<SpeechSynthesisImpl> speech_synthesis_impl_;

  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // The set of bindings types that have been enabled for this RenderFrame.
  // See BindingsPolicy for details.
  BindingsPolicySet enabled_bindings_;

  // Parsed permissions policy header. It is parsed from blink, received during
  // DidCommitProvisionalLoad. This is constant during the whole lifetime of
  // this document.
  blink::ParsedPermissionsPolicy permissions_policy_header_;

  // Tracks the permissions policy which has been set on this frame.
  std::unique_ptr<blink::PermissionsPolicy> permissions_policy_;

  // Tracks the document policy which has been set on this frame.
  std::unique_ptr<blink::DocumentPolicy> document_policy_;

#if BUILDFLAG(IS_ANDROID)
  // An InterfaceProvider for Java-implemented interfaces that are scoped to
  // this RenderFrameHost. This provides access to interfaces implemented in
  // Java in the browser process to C++ code in the browser process.
  std::unique_ptr<service_manager::InterfaceProvider> java_interfaces_;
#endif

  // Performs Mojo capability control on this RenderFrameHost when
  // `mojo_binder_policy_applier_` is not null. Mojo binder polices will be
  // applied to interfaces that are registered with BrowserInterfaceBrokerImpl
  // and AssociatedInterfaceRegistry before invoking their binders.
  // Currently, it is non-null pointer only if this RenderFrameHost is being
  // prerendered.
  std::unique_ptr<MojoBinderPolicyApplier> mojo_binder_policy_applier_;

  // IPC-friendly token that represents this host.
  const blink::LocalFrameToken frame_token_;

  // The most recent accessibility mode received from the host's delegate in
  // `UpdateAccessibilityMode()`. This is a debugging aid for
  // https://crbug.com/326751711.
  ui::AXMode last_ax_mode_;

  // Binding to remote implementation of mojom::RenderAccessibility. Note that
  // this binding is done on-demand (in UpdateAccessibilityMode()) and will only
  // be connected (i.e. bound) to the other endpoint in the renderer while there
  // is an accessibility mode that includes |kWebContents|.
  mojo::AssociatedRemote<blink::mojom::RenderAccessibility>
      render_accessibility_;

  base::SequenceBound<RenderAccessibilityHost> render_accessibility_host_;
  ui::AXTreeID render_accessibility_host_ax_tree_id_;

  mojo::AssociatedReceiver<mojom::DomAutomationControllerHost>
      dom_automation_controller_receiver_{this};

#if BUILDFLAG(ENABLE_PPAPI)
  std::unique_ptr<RenderFrameHostImplPpapiSupport> ppapi_support_;
#endif

  KeepAliveHandleFactory keep_alive_handle_factory_;

  // For observing Network Service connection errors only. Will trigger
  // `UpdateSubresourceLoaderFactories()` and push updated factories to
  // `RenderFrame`.
  mojo::Remote<network::mojom::URLLoaderFactory>
      network_service_disconnect_handler_holder_;

  // Whether UpdateSubresourceLoaderFactories should recreate the default
  // URLLoaderFactory when handling a NetworkService crash.
  bool recreate_default_url_loader_factory_after_network_service_crash_ = false;

  // Set of isolated world origins that require a separate URLLoaderFactory
  // (e.g. for handling requests initiated by extension content scripts that
  // require relaxed CORS/ORB rules).
  //
  // TODO(crbug.com/40137011): Remove the field below once Chrome
  // Platform Apps are gone.
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

  // Tracks whether we have we already posted a task to evict this. This should
  // only ever be set/read on the outermost main frame.
  bool is_evicted_from_back_forward_cache_ = false;
  base::OneShotTimer back_forward_cache_eviction_timer_;

  // The map that stores the disabled reasons and the associated UKM source ID.
  // The reasons are given in BackForwardCache::DisableForRenderFrameHost, which
  // is a breakdown of NotRestoredReason::kDisableForRenderFrameHostCalled.
  BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap
      back_forward_cache_disabled_reasons_;

  // Tracks whether the RenderFrameHost had ever been restored from back/forward
  // cache. Should only be used for debugging purposes for crbug.com/1243541.
  // TODO(crbug.com/40195481): Remove this once the bug is fixed.
  bool was_restored_from_back_forward_cache_for_debugging_ = false;

  // Whether proactive BrowsingInstance swap is disabled for this frame or not.
  // Note that even if this is false, proactive BrowsingInstance swap still
  // might not happen on navigations on this frame due to other reasons.
  // Should only be used for testing purposes.
  bool has_test_disabled_proactive_browsing_instance_swap_ = false;

  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;

  // Whether the currently committed document is MHTML or not. It is set at
  // commit time based on the MIME type of the NavigationRequest that resulted
  // in the navigation commit. Setting the value should be based only on
  // browser side state as this value is used in security checks.
  bool is_mhtml_document_ = false;

  // Whether the currently committed document was reloaded in a particular
  // way.
  ReloadType reload_type_ = ReloadType::NONE;

  // Used to intercept DidCommit* calls in tests.
  raw_ptr<CommitCallbackInterceptor> commit_callback_interceptor_ = nullptr;

  // Whether the last interception to DidCommit* causes us to ignore the commit.
  bool did_ignore_last_commit_callback_ = false;

  // Used to hear about UnloadACK calls in tests.
  UnloadACKCallbackForTesting unload_ack_callback_;

  // Mask of the active features tracked by the scheduler used by this frame.
  // This is used only for metrics.
  // See blink::SchedulingPolicy::Feature for the meaning.
  // These values should be cleared on document commit.
  //
  // Some features are tracked in these places:
  //   * `renderer_reported_bfcache_blocking_details_` for features in the
  //      document in the renderer and the JavaScript locations that used the
  //      features.
  //   * `browser_reported_bfcache_disabling_features_counts_` for the browser
  //      features.
  //   * `DedicatedWorkerHost` for features used in dedicated workers.
  // They are tracked separately, because when the renderer updates the set of
  // features, the browser ones should persist. Also, dedicated workers might be
  // destroyed while their renderers persist.
  BackForwardCacheBlockingDetails renderer_reported_bfcache_blocking_details_;

  // Count the usage of BackForwardCacheDisablingFeature.
  base::flat_map<BackForwardCacheDisablingFeature, int>
      browser_reported_bfcache_disabling_features_counts_;

  // Holds prefetched signed exchanges for SignedExchangeSubresourcePrefetch.
  // They will be passed to the next navigation.
  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Isolation information to be used for subresources from the currently
  // committed navigation. Stores both the SiteForCookies and the
  // NetworkIsolationKey.
  //
  // This is specific to a document and should be reset on every cross-document
  // commit.
  //
  // When a new frame is created:
  // 1) If the origin of the creator frame is known, then the new frame inherits
  //    the IsolationInfo from the creator frame, similarly to the last
  //    committed origin (see the SetOriginDependentStateOfNewFrame method).
  // 2) If the origin of the creator frame is not known (e.g. in no-opener case)
  //    then the initial transient isolation info (i.e. the default value below)
  //    will be used.  This will match the opaque origin of such a frame.
  net::IsolationInfo isolation_info_ = net::IsolationInfo::CreateTransient();

  // Salt for generating frame-specific media device IDs.
  std::string media_device_id_salt_base_;

  // Keep the list of ServiceWorkerClients so that they can observe when
  // the frame goes in/out of BackForwardCache.
  // TODO(yuzus): Make this a single pointer. A frame should only have a single
  // service worker client, but probably during a navigation the old service
  // worker client is still alive when the new service worker client is created
  // and added to this vector, and the old service worker client is destroyed
  // shortly after navigation.
  std::map<std::string, base::WeakPtr<ServiceWorkerClient>>
      service_worker_clients_;
  // Keeps the track of the latest ServiceWorkerClient.
  base::WeakPtr<ServiceWorkerClient> last_committed_service_worker_client_;

  // The fenced frames owned by this document, ordered with newer fenced frames
  // being appended to the end.
  std::vector<std::unique_ptr<FencedFrame>> fenced_frames_;

  // Tracking active features in this frame, for use in figuring out whether
  // or not it can be frozen.
  std::unique_ptr<FeatureObserver> feature_observer_;

  // Force renderer process foregrounded during initial loading phase
  // (crbug/351953350).
  bool boost_render_process_for_loading_ = false;

  scoped_refptr<WebAuthRequestSecurityChecker>
      webauth_request_security_checker_;

  // Reset immediately before a RenderFrameHost is reused for hosting a new
  // document.
  //
  // Note: this is an std::optional instead of a std::unique_ptr because:
  // 1. it is always allocated
  // 2. `~RenderFrameHostImpl` destroys `document_associated_data_` which
  //    destroys any `DocumentService` objects tracking `this`. Destroying a
  //    `DocumentService` unregisters it from `this`. A std::unique_ptr's
  //    stored pointer value is (intentionally) undefined during destruction
  //    (e.g. it could be nullptr), which would cause unregistration to
  //    dereference a null pointer.
  std::optional<DocumentAssociatedData> document_associated_data_;

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
  CrossOriginOpenerPolicyAccessReportManager coop_access_report_manager_;

  // https://github.com/camillelamy/explainers/blob/master/coop_reporting.md#virtual-browsing-context-group-id
  //
  // Whenever we detect that the enforcement of a report-only COOP policy would
  // have resulted in a BrowsingInstance switch, we assign a new virtual
  // browsing context group ID to the RenderFrameHostImpl that has navigated.
  int virtual_browsing_context_group_;

  // Used to track browsing context group switches that would happen if COOP
  // had a value of same-origin-allow-popups by default.
  int soap_by_default_virtual_browsing_context_group_;

  // Navigation ID for the last committed cross-document non-bfcached navigation
  // in this RenderFrameHost.
  // TODO(crbug.com/40615943): Make this const after we have RenderDocument.
  int64_t last_committed_cross_document_navigation_id_ = -1;

  // Tracks the state of |this| RenderFrameHost from the point it is created to
  // when it gets deleted.
  LifecycleStateImpl lifecycle_state_;

  // If true, RenderFrameHost should not be actually deleted and should be left
  // stuck in pending deletion.
  bool do_not_delete_for_testing_ = false;

  // Contains NotRestoredReasons for the navigation. Gets reset whenever
  // |SendCommitNavigation()| is called.
  blink::mojom::BackForwardCacheNotRestoredReasonsPtr
      not_restored_reasons_for_testing_;

  // Embedding token for the document in this RenderFrameHost. This differs from
  // |frame_token_| in that |frame_token_| has a lifetime matching that of the
  // corresponding RenderFrameHostImpl, and is intended to be used for IPCs for
  // identifying frames just like routing IDs. |embedding_token_| has a document
  // scoped lifetime and changes on cross-document navigations.
  std::optional<base::UnguessableToken> embedding_token_;

  // Observers listening to cookie access notifications for the current document
  // in this RenderFrameHost.
  // Note: at the moment this set is not cleared when a new document is created
  // in this RenderFrameHost. This is done because the first observer is created
  // before the navigation actually commits and because the old routing-id based
  // behaved in the same way as well.
  // This problem should go away with RenderDocumentHost in any case.
  // TODO(crbug.com/40615943): Remove this warning after the RDH ships.
  mojo::ReceiverSet<network::mojom::CookieAccessObserver> cookie_observers_;

  // Observers listening to Trust Token access notifications for the current
  // document in this RenderFrameHost. Note: at the moment this set is not
  // cleared when a new document is created in this RenderFrameHost. This is
  // done because the first observer is created before the navigation actually
  // commits and because the old routing-id based behaved in the same way as
  // well. This problem should go away with RenderDocumentHost in any case.
  // TODO(crbug.com/40615943): Remove this warning after the RDH ships.
  mojo::ReceiverSet<network::mojom::TrustTokenAccessObserver>
      trust_token_observers_;

  // Observers listening to shared dictionary access notifications for the
  // current document in this RenderFrameHost. Note: at the moment this set is
  // not cleared when a new document is created in this RenderFrameHost. This is
  // done because the first observer is created before the navigation actually
  // commits and because the old routing-id based behaved in the same way as
  // well. This problem should go away with RenderDocumentHost in any case.
  // TODO(crbug.com/40615943): Remove this warning after the RDH ships.
  mojo::ReceiverSet<network::mojom::SharedDictionaryAccessObserver>
      shared_dictionary_observers_;

  mojo::ReceiverSet<device::mojom::VibrationManagerListener>
      vibration_manager_listeners_;

  // Indicates whether this frame is an outer delegate frame for some other
  // RenderFrameHost. This will be a valid ID if so, and invalid otherwise.
  FrameTreeNodeId inner_tree_main_frame_tree_node_id_;

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

  // The PolicyContainerHost for the current document, containing security
  // policies that apply to it. It should never be null if the RenderFrameHost
  // is displaying a document. Its lifetime should coincide with the lifetime of
  // the document displayed in the RenderFrameHost. It is overwritten at
  // navigation commit time in DidCommitNewDocument with the PolicyContainerHost
  // of the new document. Never set this directly, but always use
  // SetPolicyContainerHost.
  // Note: Although it is owned through a scoped_refptr, a PolicyContainerHost
  // should not be shared between different owners. The PolicyContainerHost of a
  // RenderFrameHost can be retrieven with PolicyContainerHost::FromFrameToken
  // even after the RenderFrameHost has been deleted, if there exist still some
  // keepalive for it. One keepalive is always held by the LocalFrame's
  // PolicyContainer. Cf. the documentation string of the PolicyContainerHost
  // class for more information.
  scoped_refptr<PolicyContainerHost> policy_container_host_;

  // The current document's HTTP response head. This is used by back-forward
  // cache, for navigating a second time toward the same document.
  network::mojom::URLResponseHeadPtr last_response_head_;

  std::unique_ptr<NavigationEarlyHintsManager> early_hints_manager_;

  // A counter which is incremented by one every time this RenderFrameHost sends
  // a `CommitNavigation` or `CommitFailedNavigation` IPC to the renderer.
  int commit_navigation_sent_counter_ = 0;

  // CodeCacheHost processes requests to fetch / write generated code for
  // JavaScript / WebAssembly resources.
  CodeCacheHostImpl::ReceiverSet code_cache_host_receivers_;

  // Holds the mapping of names to URLs of reporting endpoints for the current
  // document, as parsed from the Reporting-Endpoints response header. This data
  // comes directly from the structured header parser, and does not necessarily
  // represent a valid reporting configuration. This is passed to the network
  // service to set up the actual endpoint configuration once the document load
  // commits.
  base::flat_map<std::string, std::string> reporting_endpoints_;

  // This indicates whether `this` is not nested in a fenced frame, or `this` is
  // associated with a fenced frame root, or `this` is associated with an iframe
  // nested within a fenced frame.
  const FencedFrameStatus fenced_frame_status_;

  // Manages a transient affordance for this frame or subframes to open a popup.
  TransientAllowPopup transient_allow_popup_;

  // Manages a transient affordance for this frame to request fullscreen.
  blink::DelegatedCapabilityRequestToken fullscreen_request_token_;

  // The user activation state of this document.  See |UserActivationState| for
  // details on how this state is maintained.
  blink::UserActivationState user_activation_state_;

  // Similar to `user_activation_state_`, but specifically for use with
  // web-exposed history manipulation (e.g., cancelling a history navigation via
  // the Navigation API). Activated when `user_activation_state_` is activated,
  // but consumed separately when a page interrupts browser-initiated history
  // navigations (e.g., after canceled history navigations or uses of
  // CloseWatcher). Note that this is used for the navigation API and not for
  // tracking whether NavigationEntries are skippable (which uses sticky user
  // activation and the honor_sticky_activation_for_history_intervention_ bit
  // below).
  blink::HistoryUserActivationState history_user_activation_state_;

  // Manages a transient state tracking mechanism for this frame to verify focus
  // transfer across a fenced frame boundary. This is essentially tracking focus
  // capability. When focus is transferred to a new frame, this state is also
  // passed to the new frame. This can be passed to multiple frames as part of a
  // single focus event if a frame doesn't have any focusable elements.
  TransientFocusSourceUserActivation focus_source_user_activation_state_;

  // For the history manipulation intervention, main frames track whether to
  // honor the sticky user activation for creating non-skippable
  // NavigationEntries. The activation can be ignored if a same-document
  // history navigation occurs, so that documents in the page cannot create
  // more non-skippable entries after a back navigation even though activations
  // are not consumed after same-document navigations. See crbug.com/1248529
  // for more details.
  // More specifically, this field is only updated for main frames, and:
  // 1) Starts at true for every new document.
  // 2) Is set to false after a same-document back/forward navigation in any
  // frame of the page.
  // 3) Is set back to true if another user activation is received or a cross
  // document commit occurs (including a BFCache activation).

  // The value is manually reset on cross-document navigations in
  // ClearUserActivation via UpdateUserActivationState, and not via
  // DocumentAssociatedData (which does not get reset for BFCache activations).
  bool honor_sticky_activation_for_history_intervention_ = true;

  // Used to avoid sending AXTreeData to the renderer if the renderer has not
  // been told root ID yet. See UpdateAXTreeData() for more details.
  bool needs_ax_root_id_ = true;
  // Used with UpdateAXFocusDeferScope to prevent UpdateAXTreeData() while focus
  // state changes are in progress.
  int ax_defer_scope_count_ = 0;
  bool ax_update_deferred_ = false;

  // The most recent page scale factor sent by the main frame's renderer.
  // Note that the renderer uses a different mechanism to persist its page
  // scale factor when performing session history navigations (see
  // blink::PageState).
  // Conceptually this should be per-PageImpl, since it is only non-one for main
  // frames, but we need to store it here due to how the renderer sends page
  // scale change notifications. If a cross-page, same-RenderFrameHost
  // navigation occurs where both pages have the same initial scale, we will
  // not get another notification.
  // TODO(crbug.com/40615943): Revisit after RenderDocument ships.
  float page_scale_factor_ = 1.f;

  // Emit a DumpWithoutCrashing() when |this| is deleted and this flag is reset.
  bool check_deletion_for_bug_1276535_ = false;

  // Used for devtools instrumentation and trace-ability. Do not use for
  // anything else, especially to look up the RenderFrameHost
  // or FrameTreeNode instance.
  // The token is propagated to Blink's LocalFrame/RemoteFrame and the
  // values should be in sync across processes with its corresponding
  // LocalFrame. Both Blink and content/ can tag calls and requests with this
  // token in order to attribute them to the context frame.
  // |devtools_frame_token_| is only defined by the browser process and is never
  // sent back from the renderer in the control calls. The token lives in the
  // RFH because DevTools protocol expects it to be stable for the RFH lifetime,
  // and it's meant to generally be stable for the FTN lifetime, but is allowed
  // to change across MPArch activations like prerendering.
  const base::UnguessableToken devtools_frame_token_;

  // BrowserInterfaceBroker implementation through which this
  // RenderFrameHostImpl exposes document-scoped Mojo services to the currently
  // active document in the corresponding RenderFrame.
  //
  // The interfaces that can be requested from this broker are defined in the
  // content/browser/browser_interface_binders.cc file, in the functions which
  // take a `RenderFrameHostImpl*` parameter.
  //
  // `broker_` is located below other members to avoid ordering issue by access
  // them during initializing BrowserInterfaceBrokerImpl.
  BrowserInterfaceBrokerImpl<RenderFrameHostImpl, RenderFrameHost*> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // The listener should be moved from the `NavigationRequest` when committing
  // a navigation in this `RenderFrameHostImpl`. It will be owned by the
  // `RenderFrameHostImpl` and continue receiving the cookie change events until
  // the destruction of the document. See the comments of the
  // `cookie_change_listener_` in `NavigationRequest`.
  std::unique_ptr<CookieChangeListener> cookie_change_listener_;

  // If true, the renderer side widget is created after the navigation is
  // committed.
  bool waiting_for_renderer_widget_creation_after_commit_ = false;

  // Deferred shared storage operations to run after navigation commit in the
  // event of a race between navigation and subresource request(s).
  std::deque<base::OnceCallback<void(NavigationOrDocumentHandle*)>>
      deferred_shared_storage_header_callbacks_;

#if BUILDFLAG(IS_ANDROID)
  // Holds a reference to a pending remote WebAuthn RP ID validation while one
  // is ongoing. Destroying this object cancels the validation.
  std::unique_ptr<WebAuthRequestSecurityChecker::RemoteValidation>
      webauthn_remote_rp_id_validation_;
#endif

  // Tracks the page that initiates Protected Audience auction. This is set
  // when AdAuctionServiceImpl is constructed, which is when the first call to
  // Protected Audience API takes place on the frame.
  //
  // See crbug.com/1422301 for why this is needed.
  //
  // TODO(crbug.com/40615943): Once RenderDocument is launched, the `PageImpl`
  // will not change. Remove this weak pointer and corresponding verification
  // logics.
  base::WeakPtr<PageImpl> auction_initiator_page_;

  // The base auction nonce used to generate all auction nonces returned by
  // `navigator.createAuctionNonce`. This base auction nonce is generated here
  // in the browser process so that it can later verify that all auctions that
  // provide a nonce in this frame provide a nonce based on this UUID, and
  // // specifically, that all such auction nonces share the first 26
  // hexadecimal digits (of UUIDv4's 32 hexadecimal digits) with this base
  // auction nonce. The last six hexadecimal digits of this UUID are combined
  // in the renderer process with a sequential value to guarantee that each
  // nonce returned is unique.
  base::Uuid base_auction_nonce_;

  // WeakPtrFactories are the last members, to ensure they are destroyed before
  // all other fields of `this`.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_{this};

  // Unlike `weak_ptr_factory` which only invalidates when `this` is about to be
  // deleted, `render_frame_scoped_weak_ptr_factory_` is invalidated every time
  // the RenderFrame is deleted (for example, if the renderer crashes).
  base::WeakPtrFactory<RenderFrameHostImpl>
      render_frame_scoped_weak_ptr_factory_{this};
};

// Used when DCHECK_STATE_TRANSITION triggers.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& o,
    const RenderFrameHostImpl::LifecycleStateImpl& s);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
