// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

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
#include "base/containers/flat_set.h"
#include "base/containers/id_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/media/old_render_frame_audio_input_stream_factory.h"
#include "content/browser/renderer_host/media/old_render_frame_audio_output_stream_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/ax_content_node_data.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/content_security_policy/csp_context.h"
#include "content/common/download/mhtml_save_status.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_replication_state.h"
#include "content/common/image_downloader/image_downloader.mojom.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_response_headers.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/viz/public/interfaces/hit_test/input_target_client.mojom.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "third_party/blink/public/platform/dedicated_worker_factory.mojom.h"
#include "third_party/blink/public/platform/modules/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/platform/modules/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_scroll_types.h"
#include "third_party/blink/public/platform/web_sudden_termination_disabler_type.h"
#include "third_party/blink/public/web/commit_result.mojom.h"
#include "third_party/blink/public/web/devtools_agent.mojom.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "services/device/public/mojom/nfc.mojom.h"
#endif

class GURL;
struct AccessibilityHostMsg_EventBundleParams;
struct AccessibilityHostMsg_FindInPageResultParams;
struct AccessibilityHostMsg_LocationChangeParams;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;
struct FrameHostMsg_OpenURL_Params;
struct FrameMsg_TextTrackSettings_Params;
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
struct FrameHostMsg_ShowPopup_Params;
#endif

namespace base {
class ListValue;
}

namespace blink {
class AssociatedInterfaceProvider;
class AssociatedInterfaceRegistry;
struct FramePolicy;
struct WebFullscreenOptions;
struct WebScrollIntoViewParams;

namespace mojom {
class WebUsbService;
}
}  // namespace blink

namespace gfx {
class Range;
}

namespace network {
class ResourceRequestBody;
struct ResourceResponse;
}  // namespace network

namespace content {
class AuthenticatorImpl;
class FrameTree;
class FrameTreeNode;
class GeolocationServiceImpl;
class KeepAliveHandleFactory;
class MediaInterfaceProxy;
class NavigationHandleImpl;
class NavigationRequest;
class PermissionServiceContext;
class PresentationServiceImpl;
class RenderFrameHostDelegate;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class SensorProviderProxyImpl;
class TimeoutMonitor;
class WebBluetoothServiceImpl;
struct CommonNavigationParams;
struct ContextMenuParams;
struct FrameOwnerProperties;
struct PendingNavigation;
struct RequestNavigationParams;
struct ResourceTimingInfo;
struct SubresourceLoaderParams;

class MediaStreamDispatcherHost;

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      public base::SupportsUserData,
      public mojom::FrameHost,
      public BrowserAccessibilityDelegate,
      public SiteInstanceImpl::Observer,
      public service_manager::mojom::InterfaceProvider,
      public CSPContext {
 public:
  using AXTreeSnapshotCallback =
      base::OnceCallback<void(const ui::AXTreeUpdate&)>;

  // An accessibility reset is only allowed to prevent very rare corner cases
  // or race conditions where the browser and renderer get out of sync. If
  // this happens more than this many times, kill the renderer.
  static const int kMaxAccessibilityResets = 5;

  static RenderFrameHostImpl* FromID(int process_id, int routing_id);
  static RenderFrameHostImpl* FromAXTreeID(ui::AXTreeID ax_tree_id);
  static RenderFrameHostImpl* FromOverlayRoutingToken(
      const base::UnguessableToken& token);

  // Allows overriding the URLLoaderFactory creation for subresources.
  // Passing a null callback will restore the default behavior.
  // This method must be called either on the UI thread or before threads start.
  // This callback is run on the UI thread.
  using CreateNetworkFactoryCallback = base::Callback<void(
      network::mojom::URLLoaderFactoryRequest request,
      int process_id,
      network::mojom::URLLoaderFactoryPtrInfo original_factory)>;
  static void SetNetworkFactoryForTesting(
      const CreateNetworkFactoryCallback& url_loader_factory_callback);

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  int GetRoutingID() override;
  ui::AXTreeID GetAXTreeID() override;
  SiteInstanceImpl* GetSiteInstance() override;
  RenderProcessHost* GetProcess() override;
  RenderWidgetHostView* GetView() override;
  RenderFrameHostImpl* GetParent() override;
  bool IsDescendantOf(RenderFrameHost*) override;
  int GetFrameTreeNodeId() override;
  base::UnguessableToken GetDevToolsFrameToken() override;
  const std::string& GetFrameName() override;
  bool IsCrossProcessSubframe() override;
  const GURL& GetLastCommittedURL() override;
  const url::Origin& GetLastCommittedOrigin() override;
  gfx::NativeView GetNativeView() override;
  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message) override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         const JavaScriptResultCallback& callback) override;
  void ExecuteJavaScriptInIsolatedWorld(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback,
      int world_id) override;
  void ExecuteJavaScriptForTests(const base::string16& javascript) override;
  void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback) override;
  void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  void InsertVisualStateCallback(VisualStateCallback callback) override;
  void CopyImageAt(int x, int y) override;
  void SaveImageAt(int x, int y) override;
  RenderViewHost* GetRenderViewHost() override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  blink::mojom::PageVisibilityState GetVisibilityState() override;
  bool IsRenderFrameLive() override;
  bool IsCurrent() override;
  int GetProxyCount() override;
  bool HasSelection() override;
  void RequestTextSurroundingSelection(
      const TextSurroundingSelectionCallback& callback,
      int max_length) override;
  void AllowBindings(int binding_flags) override;
  int GetEnabledBindings() const override;
  void DisableBeforeUnloadHangMonitorForTesting() override;
  bool IsBeforeUnloadHangMonitorDisabledForTesting() override;
  bool GetSuddenTerminationDisablerState(
      blink::WebSuddenTerminationDisablerType disabler_type) override;
  bool IsFeatureEnabled(blink::mojom::FeaturePolicyFeature feature) override;
  void ViewSource() override;
  blink::mojom::PauseSubresourceLoadingHandlePtr PauseSubresourceLoading()
      override;
  void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point&,
      const blink::WebMediaPlayerAction& action) override;
  bool CreateNetworkServiceDefaultFactory(
      network::mojom::URLLoaderFactoryRequest default_factory_request) override;
  void MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
      std::vector<url::Origin> request_initiators,
      bool push_to_renderer_now) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // BrowserAccessibilityDelegate
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() const override;
  gfx::Rect AccessibilityGetViewBounds() const override;
  gfx::Point AccessibilityOriginInScreen(
      const gfx::Rect& bounds) const override;
  float AccessibilityGetDeviceScaleFactor() const override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;

  // SiteInstanceImpl::Observer
  void RenderProcessGone(SiteInstanceImpl* site_instance) override;

  // CSPContext
  void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params) override;
  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override;
  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      CSPDirective::Name directive,
      GURL* blocked_url,
      SourceLocation* source_location) const override;

  mojom::FrameInputHandler* GetFrameInputHandler();

  viz::mojom::InputTargetClient* GetInputTargetClient() {
    return input_target_client_;
  }

  // Creates a RenderFrame in the renderer process.
  bool CreateRenderFrame(int proxy_routing_id,
                         int opener_routing_id,
                         int parent_routing_id,
                         int previous_sibling_routing_id);

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  This is currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  void SetRenderFrameCreated(bool created);

  // Called for renderer-created windows to resume requests from this frame,
  // after they are blocked in RenderWidgetHelper::CreateNewWindow.
  void Init();

  // Returns true if the frame recently plays an audio.
  bool is_audible() const { return is_audible_; }
  void OnAudibleStateChanged(bool is_audible);

  int routing_id() const { return routing_id_; }

  // Called when this frame has added a child. This is a continuation of an IPC
  // that was partially handled on the IO thread (to allocate |new_routing_id|
  // and |devtools_frame_token|), and is forwarded here. The renderer has
  // already been told to create a RenderFrame with the specified ID values.
  // |interface_provider_request| is the request end of the InterfaceProvider
  // interface that the RenderFrameHost corresponding to the child frame should
  // bind to expose services to the renderer process. The caller takes care of
  // sending down the client end of the pipe to the child RenderFrame to use.
  void OnCreateChildFrame(int new_routing_id,
                          service_manager::mojom::InterfaceProviderRequest
                              interface_provider_request,
                          blink::WebTreeScopeType scope,
                          const std::string& frame_name,
                          const std::string& frame_unique_name,
                          bool is_created_by_script,
                          const base::UnguessableToken& devtools_frame_token,
                          const blink::FramePolicy& frame_policy,
                          const FrameOwnerProperties& frame_owner_properties,
                          blink::FrameOwnerElementType owner_type);

  // Update this frame's state at the appropriate time when a navigation
  // commits. This is called by NavigatorImpl::DidNavigate as a helper, in the
  // midst of a DidCommitProvisionalLoad call.
  void DidNavigate(const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
                   bool is_same_document_navigation);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // Methods to add/remove/reset/query child FrameTreeNodes of this frame.
  // See class-level comment for FrameTreeNode for how the frame tree is
  // represented.
  size_t child_count() { return children_.size(); }
  FrameTreeNode* child_at(size_t index) const { return children_[index].get(); }
  FrameTreeNode* AddChild(std::unique_ptr<FrameTreeNode> child,
                          int process_id,
                          int frame_routing_id);
  void RemoveChild(FrameTreeNode* child);
  void ResetChildren();

  // Allows FrameTreeNode::SetCurrentURL to update this frame's last committed
  // URL.  Do not call this directly, since we rely on SetCurrentURL to track
  // whether a real load has committed or not.
  void SetLastCommittedUrl(const GURL& url);

  // The most recent non-net-error URL to commit in this frame.  In almost all
  // cases, use GetLastCommittedURL instead.
  const GURL& last_successful_url() { return last_successful_url_; }

  // Allows overriding the last committed origin in tests.
  void SetLastCommittedOriginForTesting(const url::Origin& origin);

  // Fetch the link-rel canonical URL to be used for sharing to external
  // applications.
  void GetCanonicalUrlForSharing(
      mojom::Frame::GetCanonicalUrlForSharingCallback callback);

  // Returns the associated WebUI or null if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }

  // Returns the pending WebUI, or null if none applies.
  WebUIImpl* pending_web_ui() const {
    return should_reuse_web_ui_ ? web_ui_.get() : pending_web_ui_.get();
  }

  // Returns this RenderFrameHost's loading state. This method is only used by
  // FrameTreeNode. The proper way to check whether a frame is loading is to
  // call FrameTreeNode::IsLoading.
  bool is_loading() const { return is_loading_; }

  // Returns true if this is a top-level frame, or if this frame's RenderFrame
  // is in a different process from its parent frame. Local roots are
  // distinguished by owning a RenderWidgetHost, which manages input events
  // and painting for this frame and its contiguous local subtree in the
  // renderer process.
  bool is_local_root() const { return !!render_widget_host_; }

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

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.
  int nav_entry_id() const { return nav_entry_id_; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

  // A NavigationRequest for a pending cross-document navigation in this frame,
  // if any. This is cleared when the navigation commits.
  NavigationRequest* navigation_request() { return navigation_request_.get(); }

  // A NavigationRequest for a pending same-document navigation in this frame,
  // if any. This is cleared when the navigation commits.
  NavigationRequest* same_document_navigation_request() {
    return same_document_navigation_request_.get();
  }

  // Returns the NavigationHandleImpl stored in the NavigationRequest returned
  // by GetNavigationRequest(), if any.
  NavigationHandleImpl* GetNavigationHandle();

  // Resets the NavigationRequests stored in this RenderFrameHost.
  void ResetNavigationRequests();

  // Called when a navigation is ready to commit in this
  // RenderFrameHost. Transfers ownership of the NavigationRequest associated
  // with the navigation to this RenderFrameHost.
  void SetNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Tells the renderer that this RenderFrame is being swapped out for one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // RenderFrameProxy to replace the RenderFrame and set it to |is_loading|
  // state. The renderer should preserve the RenderFrameProxy object until it
  // exits, in case we come back.  The renderer can exit if it has no other
  // active RenderFrames, but not until WasSwappedOut is called.
  void SwapOut(RenderFrameProxyHost* proxy, bool is_loading);

  // Whether an ongoing navigation in this frame is waiting for a BeforeUnload
  // ACK either from this RenderFrame or from one of its subframes.
  bool is_waiting_for_beforeunload_ack() const {
    return is_waiting_for_beforeunload_ack_;
  }

  // Whether the RFH is waiting for an unload ACK from the renderer.
  bool IsWaitingForUnloadACK() const;

  // Called when either the SwapOut request has been acknowledged or has timed
  // out.
  void OnSwappedOut();

  // This method returns true from the time this RenderFrameHost is created
  // until SwapOut is called, at which point it is pending deletion.
  bool is_active() { return !is_waiting_for_swapout_ack_; }

  // Navigates to an interstitial page represented by the provided data URL.
  void NavigateToInterstitialURL(const GURL& data_url);

  // Stop the load in progress.
  void Stop();

  enum class BeforeUnloadType {
    BROWSER_INITIATED_NAVIGATION,
    RENDERER_INITIATED_NAVIGATION,
    TAB_CLOSE,
    // This reason is used before a tab is discarded in order to free up
    // resources. When this is used and the handler returns a non-empty string,
    // the confirmation dialog will not be displayed and the discard will
    // automatically be canceled.
    DISCARD,
  };

  // Runs the beforeunload handler for this frame and its subframes. |type|
  // indicates whether this call is for a navigation or tab close. |is_reload|
  // indicates whether the navigation is a reload of the page.  If |type|
  // corresponds to tab close and not a navigation, |is_reload| should be
  // false.
  void DispatchBeforeUnload(BeforeUnloadType type, bool is_reload);

  // Simulate beforeunload ack on behalf of renderer if it's unrenresponsive.
  void SimulateBeforeUnloadAck(bool proceed);

  // Returns true if a call to DispatchBeforeUnload will actually send the
  // BeforeUnload IPC.  This can be called on a main frame or subframe.  If
  // |check_subframes_only| is false, it covers handlers for the frame
  // itself and all its descendants.  If |check_subframes_only| is true, it
  // only checks the frame's descendants but not the frame itself.
  bool ShouldDispatchBeforeUnload(bool check_subframes_only);

  // Allow tests to override how long to wait for beforeunload ACKs to arrive
  // before timing out.
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
  void AdvanceFocus(blink::WebFocusType type,
                    RenderFrameProxyHost* source_proxy);

  // Notifies the RenderFrame that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input);

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
  // renderer process, and the accessibility tree it sent can be
  // retrieved using GetAXTreeForTesting().
  void SetAccessibilityCallbackForTesting(
      const base::Callback<void(RenderFrameHostImpl*, ax::mojom::Event, int)>&
          callback);

  // Called when the metadata about the accessibility tree for this frame
  // changes due to a browser-side change, as opposed to due to an IPC from
  // a renderer.
  void UpdateAXTreeData();

  // Set the AX tree ID of the embedder RFHI, if this is a browser plugin guest.
  void set_browser_plugin_embedder_ax_tree_id(ui::AXTreeID ax_tree_id) {
    browser_plugin_embedder_ax_tree_id_ = ax_tree_id;
  }

  // Send a message to the render process to change text track style settings.
  void SetTextTrackSettings(const FrameMsg_TextTrackSettings_Params& params);

  // Returns a snapshot of the accessibility tree received from the
  // renderer as of the last time an accessibility notification was
  // received.
  const ui::AXTree* GetAXTreeForTesting();

  // Access the BrowserAccessibilityManager if it already exists.
  BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // NULL.
  BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  // Select popup menu related methods (for external popup menus).
  void DidSelectPopupMenuItem(int selected_index);
  void DidCancelPopupMenu();
#else
  void DidSelectPopupMenuItems(const std::vector<int>& selected_indices);
  void DidCancelPopupMenu();
#endif
#endif

  // Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  // |subresource_loader_params| is used in network service land to pass
  // the parameters to create a custom subresource loader in the renderer
  // process, e.g. by AppCache etc.
  // TODO(clamy): Pass the NavigationRequest directly to this function when
  // interstitials have been refactored to no longer call CommitNavigation
  // without a NavigationRequest.
  void CommitNavigation(
      int64_t navigation_id,
      network::ResourceResponse* response,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      bool is_view_source,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      const base::UnguessableToken& devtools_navigation_token);

  // Indicates that a navigation failed and that this RenderFrame should display
  // an error page.
  void FailedNavigation(int64_t navigation_id,
                        const CommonNavigationParams& common_params,
                        const RequestNavigationParams& request_params,
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

  // Updates the pending WebUI of this RenderFrameHost based on the provided
  // |dest_url|, setting it to either none, a new instance or to reuse the
  // currently active one. Returns true if the pending WebUI was updated.
  // If this is a history navigation its NavigationEntry bindings should be
  // provided through |entry_bindings| to allow verifying that they are not
  // being set differently this time around. Otherwise |entry_bindings| should
  // be set to NavigationEntryImpl::kInvalidBindings so that no checks are done.
  bool UpdatePendingWebUI(const GURL& dest_url, int entry_bindings);

  // Updates the active WebUI with the pending one set by the last call to
  // UpdatePendingWebUI and then clears any pending data. If UpdatePendingWebUI
  // was not called the active WebUI will simply be cleared.
  void CommitPendingWebUI();

  // Destroys the pending WebUI and resets related data.
  void ClearPendingWebUI();

  // Destroys all WebUI instances and resets related data.
  void ClearAllWebUI();

  // Returns the Mojo ImageDownloader service.
  const content::mojom::ImageDownloaderPtr& GetMojoImageDownloader();

  // Returns pointer to renderer side FindInPage associated with this frame.
  const blink::mojom::FindInPageAssociatedPtr& GetFindInPage();

  resource_coordinator::FrameResourceCoordinator* GetFrameResourceCoordinator()
      override;

  // Resets the loading state. Following this call, the RenderFrameHost will be
  // in a non-loading state.
  void ResetLoadingState();

  // Returns the feature policy which should be enforced on this RenderFrame.
  blink::FeaturePolicy* feature_policy() { return feature_policy_.get(); }

  // Tells the renderer that this RenderFrame will soon be swapped out, and thus
  // not to create any new modal dialogs until it happens.  This must be done
  // separately so that the ScopedPageLoadDeferrers of any current dialogs are
  // no longer on the stack when we attempt to swap it out.
  void SuppressFurtherDialogs();

  void ClearFocusedElement();

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  // Returns the PreviewsState of the last successful navigation
  // that made a network request. The PreviewsState is a bitmask of potentially
  // several Previews optimizations.
  PreviewsState last_navigation_previews_state() const {
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
  void BindDevToolsAgent(blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
                         blink::mojom::DevToolsAgentAssociatedRequest request);

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHost();
  service_manager::InterfaceProvider* GetJavaInterfaces() override;
#endif

  // Propagates the visibility state along the immediate local roots by calling
  // RenderWidgetHostViewChildFrame::Show()/Hide(). Calling this on a pending
  // or speculative RenderFrameHost (that has not committed) should be avoided.
  void SetVisibilityForChildViews(bool visible);

  // Returns an unguessable token for this RFHI.  This provides a temporary way
  // to identify a RenderFrameHost that's compatible with IPC.  Else, one needs
  // to send pid + RoutingID, but one cannot send pid.  One can get it from the
  // channel, but this makes it much harder to get wrong.
  // Once media switches to mojo, we should be able to remove this in favor of
  // sending a mojo overlay factory.
  const base::UnguessableToken& GetOverlayRoutingToken();

  // Binds the request end of the InterfaceProvider interface through which
  // services provided by this RenderFrameHost are exposed to the correponding
  // RenderFrame. The caller is responsible for plumbing the client end to the
  // the renderer process.
  void BindInterfaceProviderRequest(
      service_manager::mojom::InterfaceProviderRequest
          interface_provider_request);

  // Exposed so that tests can swap out the implementation and intercept calls.
  mojo::AssociatedBinding<mojom::FrameHost>& frame_host_binding_for_testing() {
    return frame_host_associated_binding_;
  }

  mojo::Binding<service_manager::mojom::InterfaceProvider>&
  document_scoped_interface_provider_binding_for_testing() {
    return document_scoped_interface_provider_binding_;
  }
  void SetKeepAliveTimeoutForTesting(base::TimeDelta timeout);

  blink::WebSandboxFlags active_sandbox_flags() {
    return active_sandbox_flags_;
  }

  // Calls |FlushForTesting()| on Network Service and FrameNavigationControl
  // related interfaces to make sure all in-flight mojo messages have been
  // received by the other end. For test use only.
  void FlushNetworkAndNavigationInterfacesForTesting();

  // Notifies the render frame about a user activation from the browser side.
  void NotifyUserActivation();

  // Returns the current size for this frame.
  const base::Optional<gfx::Size>& frame_size() const { return frame_size_; }

  // Re-creates loader factories and pushes them to |RenderFrame|.
  // Used in case we need to add or remove intercepting proxies to the
  // running renderer, or in case of Network Service connection errors.
  void UpdateSubresourceLoaderFactories();

  // Allow tests to override the timeout used to keep subframe processes alive
  // for unload handler processing.
  void SetSubframeUnloadTimeoutForTesting(const base::TimeDelta& timeout);

  // Returns the list of NavigationEntry ids corresponding to NavigationRequests
  // waiting to commit in this RenderFrameHost.
  std::set<int> GetNavigationEntryIdsPendingCommit();

  void DidCommitProvisionalLoadForTesting(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      service_manager::mojom::InterfaceProviderRequest
          interface_provider_request);

  service_manager::BinderRegistry& BinderRegistryForTesting() {
    return *registry_;
  }

  // Called when the WebAudio AudioContext given by |audio_context_id| has
  // started (or stopped) playing audible audio.
  void AudioContextPlaybackStarted(int audio_context_id);
  void AudioContextPlaybackStopped(int audio_context_id);

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(SiteInstance* site_instance,
                      RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int32_t routing_id,
                      int32_t widget_routing_id,
                      bool hidden,
                      bool renderer_initiated_creation);

 private:
  friend class RenderFrameHostFeaturePolicyTest;
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;

  FRIEND_TEST_ALL_PREFIXES(NavigatorTestWithBrowserSideNavigation,
                           TwoNavigationsRacingCommit);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           SubframeShowsDialogWhenMainFrameNavigates);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           TimerNotRestartedBySecondDialog);
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
                           WebUIJavascriptDisallowedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, LastCommittedOrigin);
  FRIEND_TEST_ALL_PREFIXES(
      RenderFrameHostManagerUnloadBrowserTest,
      PendingDeleteRFHProcessShutdownDoesNotRemoveSubframes);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrashSubframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, FindImmediateLocalRoots);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostIsNotReusedAfterDelayedSwapOutACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostStaysActiveWithLateSwapoutACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LoadEventForwardingWhilePendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ContextMenuAfterCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ActiveSandboxFlagsRetainedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LastCommittedURLRetainedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderFrameProxyNotRecreatedDuringProcessShutdown);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           SwapOutACKArrivesPriorToProcessShutdownRequest);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderViewHost);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           FullscreenAfterFrameSwap);

  class DroppedInterfaceRequestLogger;

  // IPC Message handlers.
  void OnDidAddMessageToConsole(int32_t level,
                                const base::string16& message,
                                int32_t line_no,
                                const base::string16& source_id);
  void OnDetach();
  void OnFrameFocused();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnDocumentOnLoadCompleted();
  void OnDidFailProvisionalLoadWithError(
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidFailLoadWithError(const GURL& url,
                              int error_code,
                              const base::string16& error_description);
  void OnUpdateState(const PageState& state);
  void OnBeforeUnloadACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnSwapOutACK();
  void OnRenderProcessGone(int status, int error_code);
  void OnContextMenu(const ContextMenuParams& params);
  void OnJavaScriptExecuteResponse(int id, const base::ListValue& result);
  void OnVisualStateResponse(uint64_t id);
  void OnRunJavaScriptDialog(const base::string16& message,
                             const base::string16& default_prompt,
                             JavaScriptDialogType dialog_type,
                             IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(bool is_reload, IPC::Message* reply_msg);
  void OnRunFileChooser(const blink::mojom::FileChooserParams& params);
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          uint32_t start_offset,
                                          uint32_t end_offset);
  void OnDidAccessInitialDocument();
  void OnDidChangeOpener(int32_t opener_routing_id);

  // A new set of CSP |policies| has been added to the document.
  void OnDidAddContentSecurityPolicies(
      const std::vector<ContentSecurityPolicy>& policies);

  void OnDidChangeFramePolicy(int32_t frame_routing_id,
                              const blink::FramePolicy& frame_policy);
  void OnDidChangeFrameOwnerProperties(int32_t frame_routing_id,
                                       const FrameOwnerProperties& properties);
  void OnUpdateTitle(const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnDidBlockFramebust(const GURL& url);
  // Only used with PerNavigationMojoInterface disabled.
  void OnAbortNavigation();
  void OnForwardResourceTimingToParent(
      const ResourceTimingInfo& resource_timing);
  void OnDispatchLoad();
  void OnAccessibilityEvents(
      const AccessibilityHostMsg_EventBundleParams& params,
      int reset_token,
      int ack_token);
  void OnAccessibilityLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);
  void OnAccessibilityFindInPageResult(
      const AccessibilityHostMsg_FindInPageResultParams& params);
  void OnAccessibilityChildFrameHitTestResult(
      int action_request_id,
      const gfx::Point& point,
      int child_frame_routing_id,
      int child_frame_browser_plugin_instance_id,
      ax::mojom::Event event_to_fire);
  void OnAccessibilitySnapshotResponse(
      int callback_id,
      const AXContentTreeUpdate& snapshot);
  void OnEnterFullscreen(const blink::WebFullscreenOptions& options);
  void OnExitFullscreen();
  void OnSuddenTerminationDisablerChanged(
      bool present,
      blink::WebSuddenTerminationDisablerType disabler_type);
  void OnDidStopLoading();
  void OnDidChangeLoadProgress(double load_progress);
  void OnSerializeAsMHTMLResponse(
      int job_id,
      MhtmlSaveStatus save_status,
      const std::set<std::string>& digests_of_uris_of_serialized_resources,
      base::TimeDelta renderer_main_thread_time);
  void OnSelectionChanged(const base::string16& text,
                          uint32_t offset,
                          const gfx::Range& range);
  void OnFocusedNodeChanged(bool is_editable_element,
                            const gfx::Rect& bounds_in_frame_widget);
  void OnUpdateUserActivationState(blink::UserActivationUpdateType update_type);
  void OnSetHasReceivedUserGestureBeforeNavigation(bool value);
  void OnScrollRectToVisibleInParentFrame(
      const gfx::Rect& rect_to_scroll,
      const blink::WebScrollIntoViewParams& params);
  void OnBubbleLogicalScrollInParentFrame(
      blink::WebScrollDirection direction,
      blink::WebScrollGranularity granularity);
  void OnFrameDidCallFocus();
  void OnRenderFallbackContentInParentProcess();

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void OnShowPopup(const FrameHostMsg_ShowPopup_Params& params);
  void OnHidePopup();
#endif
#if defined(OS_ANDROID)
  void ForwardGetInterfaceToRenderFrame(const std::string& interface_name,
                                        mojo::ScopedMessagePipeHandle pipe);
#endif

  // Called when the frame would like an overlay routing token.  This will
  // create one if needed.  Either way, it will send it to the frame.
  void OnRequestOverlayRoutingToken();

  void OnShowCreatedWindow(int pending_widget_routing_id,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_rect,
                           bool user_gesture);

  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       CreateNewWindowCallback callback) override;
  void IssueKeepAliveHandle(mojom::KeepAliveHandleRequest request) override;
  void DidCommitProvisionalLoad(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
          validated_params,
      service_manager::mojom::InterfaceProviderRequest
          interface_provider_request) override;
  void DidCommitSameDocumentNavigation(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
          validated_params) override;
  void BeginNavigation(
      const CommonNavigationParams& common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      mojom::NavigationClientAssociatedPtrInfo navigation_client,
      blink::mojom::NavigationInitiatorPtr navigation_initiator) override;
  void SubresourceResponseStarted(const GURL& url,
                                  net::CertStatus cert_status) override;
  void ResourceLoadComplete(
      mojom::ResourceLoadInfoPtr resource_load_info) override;
  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override;
  void EnforceInsecureRequestPolicy(
      blink::WebInsecureRequestPolicy policy) override;
  void EnforceInsecureNavigationsSet(const std::vector<uint32_t>& set) override;
  void DidSetFramePolicyHeaders(
      blink::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& parsed_header) override;
  void CancelInitialHistoryLoad() override;
  void UpdateEncoding(const std::string& encoding) override;
  void FrameSizeChanged(const gfx::Size& frame_size) override;
  void FullscreenStateChanged(bool is_fullscreen) override;
#if defined(OS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override;
#endif

  // Registers Mojo interfaces that this frame host makes available.
  void RegisterMojoInterfaces();

  // Resets any waiting state of this RenderFrameHost that is no longer
  // relevant.
  void ResetWaitingState();

  // Returns whether the given origin is allowed to commit in the current
  // RenderFrameHost. The |url| is used to ensure it matches the origin in cases
  // where it is applicable. This is a more conservative check than
  // RenderProcessHost::FilterURL, since it will be used to kill processes that
  // commit unauthorized origins.
  bool CanCommitOrigin(const url::Origin& origin, const GURL& url);

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
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params);

  // Creates a Network Service-backed factory from appropriate |NetworkContext|
  // and sets a connection error handler to trigger
  // |OnNetworkServiceConnectionError()| if the factory is out-of-process.  If
  // this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.
  //
  // |origin| is the origin that the RenderFrame is either committing (in the
  // case of navigation) or has last committed (when handling network process
  // crashes).
  bool CreateNetworkServiceDefaultFactoryAndObserve(
      const url::Origin& origin,
      network::mojom::URLLoaderFactoryRequest default_factory_request);

  // |origin| is the origin that the RenderFrame is either committing (in the
  // case of navigation) or has last committed (when handling network process
  // crashes).
  bool CreateNetworkServiceDefaultFactoryInternal(
      const url::Origin& origin,
      network::mojom::URLLoaderFactoryRequest default_factory_request);

  // Returns true if the ExecuteJavaScript() API can be used on this host.
  bool CanExecuteJavaScript();

  // Map a routing ID from a frame in the same frame tree to a globally
  // unique AXTreeID.
  ui::AXTreeID RoutingIDToAXTreeID(int routing_id);

  // Map a browser plugin instance ID to the AXTreeID of the plugin's
  // main frame.
  ui::AXTreeID BrowserPluginInstanceIDToAXTreeID(int routing_id);

  // Convert the content-layer-specific AXContentNodeData to a general-purpose
  // AXNodeData structure.
  void AXContentNodeDataToAXNodeData(const AXContentNodeData& src,
                                     ui::AXNodeData* dst);

  // Convert the content-layer-specific AXContentTreeData to a general-purpose
  // AXTreeData structure.
  void AXContentTreeDataToAXTreeData(ui::AXTreeData* dst);

  // Returns the RenderWidgetHostView used for accessibility. For subframes,
  // this function will return the platform view on the main frame; for main
  // frames, it will return the current frame's view.
  RenderWidgetHostViewBase* GetViewForAccessibility();

  // Returns the child FrameTreeNode if |child_frame_routing_id| is an
  // immediate child of this FrameTreeNode.  |child_frame_routing_id| is
  // considered untrusted, so the renderer process is killed if it refers to a
  // FrameTreeNode that is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(int32_t child_frame_routing_id,
                                    bad_message::BadMessageReason reason);

  // Creates Web Bluetooth Service owned by the frame. Returns a raw pointer
  // to it.
  WebBluetoothServiceImpl* CreateWebBluetoothService(
      blink::mojom::WebBluetoothServiceRequest request);

  // Deletes the Web Bluetooth Service owned by the frame.
  void DeleteWebBluetoothService(
      WebBluetoothServiceImpl* web_bluetooth_service);

  // Creates connections to WebUSB interfaces bound to this frame.
  void CreateWebUsbService(
      mojo::InterfaceRequest<blink::mojom::WebUsbService> request);

  void CreateAudioInputStreamFactory(
      mojom::RendererAudioInputStreamFactoryRequest request);
  void CreateAudioOutputStreamFactory(
      mojom::RendererAudioOutputStreamFactoryRequest request);

  void CreateMediaStreamDispatcherHost(
      MediaStreamManager* media_stream_manager,
      mojom::MediaStreamDispatcherHostRequest request);

  void BindMediaInterfaceFactoryRequest(
      media::mojom::InterfaceFactoryRequest request);

  void CreateWebSocket(network::mojom::WebSocketRequest request);

  void CreateDedicatedWorkerHostFactory(
      blink::mojom::DedicatedWorkerFactoryRequest request);

  // Callback for connection error on the media::mojom::InterfaceFactory client.
  void OnMediaInterfaceFactoryConnectionError();

  void BindWakeLockRequest(device::mojom::WakeLockRequest request);

#if defined(OS_ANDROID)
  void BindNFCRequest(device::mojom::NFCRequest request);
#endif

  void BindPresentationServiceRequest(
      blink::mojom::PresentationServiceRequest request);

#if !defined(OS_ANDROID)
  void BindAuthenticatorRequest(blink::mojom::AuthenticatorRequest request);
#endif

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Allows tests to disable the swapout event timer to simulate bugs that
  // happen before it fires (to avoid flakiness).
  void DisableSwapOutTimerForTesting();

  void SendJavaScriptDialogReply(IPC::Message* reply_msg,
                                 bool success,
                                 const base::string16& user_input);

  // Returns ownership of the NavigationHandle associated with a navigation that
  // just committed.
  std::unique_ptr<NavigationHandleImpl>
  TakeNavigationHandleForSameDocumentCommit(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);
  std::unique_ptr<NavigationHandleImpl> TakeNavigationHandleForCommit(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);

  // Helper to process the beforeunload ACK. |proceed| indicates whether the
  // navigation or tab close should be allowed to proceed.  If
  // |treat_as_final_ack| is true, the frame should stop waiting for any
  // further ACKs from subframes. ACKs received from the renderer set
  // |treat_as_final_ack| to false, whereas a beforeunload timeout sets it to
  // true.
  void ProcessBeforeUnloadACK(
      bool proceed,
      bool treat_as_final_ack,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);

  // Find the frame that triggered the beforeunload handler to run in this
  // frame, which might be the frame itself or its ancestor.  This will
  // return the frame that is navigating, or the main frame if beforeunload was
  // triggered by closing the current tab.  It will return null if no
  // beforeunload is currently in progress.
  RenderFrameHostImpl* GetBeforeUnloadInitiator();

  // Called when a particular frame finishes running a beforeunload handler,
  // possibly as part of processing beforeunload for an ancestor frame.  In
  // that case, this is called on the ancestor frame that is navigating or
  // closing, and |frame| indicates which beforeunload ACK is received.  If a
  // beforeunload timeout occurred, |treat_as_final_ack| is set to true.
  // |is_frame_being_destroyed| is set to true if this was called as part of
  // destroying |frame|.
  void ProcessBeforeUnloadACKFromFrame(
      bool proceed,
      bool treat_as_final_ack,
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

  // Called when a navigation commits succesfully to |url|. This will update
  // |last_committed_site_url_| if it's not equal to the site url corresponding
  // to |url|.
  void SetLastCommittedSiteUrl(const GURL& url);

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
      const base::Callback<void(RenderFrameHostImpl*)>& callback);

  // Lazily initializes and returns the mojom::FrameNavigationControl interface
  // for this frame. May be overridden by friend subclasses for e.g. tests which
  // wish to intercept outgoing navigation control messages.
  virtual mojom::FrameNavigationControl* GetNavigationControl();

  // Utility function used to validate potentially harmful parameters sent by
  // the renderer during the commit notification.
  // A return value of true means that the commit should proceed.
  bool ValidateDidCommitParams(
      FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params);

  // Updates the site url if the navigation was successful and the page is not
  // an interstitial.
  void UpdateSiteURL(const GURL& url, bool url_is_unreachable);

  // Called when we receive the confirmation that a navigation committed in the
  // renderer. Used by both DidCommitSameDocumentNavigation and
  // DidCommitNavigation.
  // Returns true if the navigation did commit properly, false if the commit
  // state should be restored to its pre-commit value.
  bool DidCommitNavigationInternal(
      FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params,
      bool is_same_document_navigation);

  // Called by the renderer process when it is done processing a same-document
  // commit request.
  void OnSameDocumentCommitProcessed(int64_t navigation_id,
                                     bool should_replace_current_entry,
                                     blink::mojom::CommitResult result);

  // Called by the renderer process when it is done processing a cross-document
  // commit request.
  void OnCrossDocumentCommitProcessed(int64_t navigation_id,
                                      blink::mojom::CommitResult result);

  // Saves and clones URLLoaderFactoryBundleInfo for subresource loading.
  // Must be called every time subresource_factories_bundle is updated.
  void SaveSubresourceFactories(
      std::unique_ptr<URLLoaderFactoryBundleInfo> bundle_info);
  std::unique_ptr<URLLoaderFactoryBundleInfo> CloneSubresourceFactories();

  // Creates a TracedValue object containing the details of a committed
  // navigation, so it can be logged with the tracing system.
  std::unique_ptr<base::trace_event::TracedValue> CommitAsTracedValue(
      FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params) const;

  // Creates initiator-specific URLLoaderFactory objects for intiator origins
  // registered via MarkInitiatorAsRequiringSeparateURLLoaderFactory method.
  URLLoaderFactoryBundleInfo::OriginMap
  CreateInitiatorSpecificURLLoaderFactories();

  // For now, RenderFrameHosts indirectly keep RenderViewHosts alive via a
  // refcount that calls Shutdown when it reaches zero.  This allows each
  // RenderFrameHostManager to just care about RenderFrameHosts, while ensuring
  // we have a RenderViewHost for each RenderFrameHost.
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  RenderViewHostImpl* const render_view_host_;

  RenderFrameHostDelegate* const delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  const scoped_refptr<SiteInstanceImpl> site_instance_;

  // The renderer process this RenderFrameHost is associated with. It is
  // initialized through a call to site_instance_->GetProcess() at creation
  // time. RenderFrameHost::GetProcess() uses this cached pointer to avoid
  // recreating the renderer process if it has crashed, since using
  // SiteInstance::GetProcess() has the side effect of creating the process
  // again if it is gone.
  RenderProcessHost* const process_;

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
  // deletion, and GetParent() should never return a different value.
  RenderFrameHostImpl* parent_;

  // Track this frame's last committed URL.
  GURL last_committed_url_;

  // Track this frame's last committed origin.
  url::Origin last_committed_origin_;

  // Track the site URL of the last site we committed successfully, as obtained
  // from SiteInstance::GetSiteURL.
  GURL last_committed_site_url_;

  // The most recent non-error URL to commit in this frame.  Remove this in
  // favor of GetLastCommittedURL() once PlzNavigate is enabled or cross-process
  // transfers work for net errors.  See https://crbug.com/588314.
  GURL last_successful_url_;

  // The mapping of pending JavaScript calls created by
  // ExecuteJavaScript and their corresponding callbacks.
  std::map<int, JavaScriptResultCallback> javascript_callbacks_;
  std::map<uint64_t, VisualStateCallback> visual_state_callbacks_;

  // RenderFrameHosts that need management of the rendering and input events
  // for their frame subtrees require RenderWidgetHosts. This typically
  // means frames that are rendered in different processes from their parent
  // frames.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  RenderWidgetHostImpl* render_widget_host_;

  const int routing_id_;

  // Boolean indicating whether this RenderFrameHost is being actively used or
  // is waiting for FrameHostMsg_SwapOut_ACK and thus pending deletion.
  bool is_waiting_for_swapout_ack_;

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  Currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  bool render_frame_created_;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Set to true when there is a pending FrameMsg_BeforeUnload message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_ack_;

  // Valid only when |is_waiting_for_beforeunload_ack_| is true. This indicates
  // whether a subsequent request to launch a modal dialog should be honored or
  // whether it should implicitly cause the unload to be canceled.
  bool beforeunload_dialog_request_cancels_unload_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true).
  bool unload_ack_is_for_navigation_;

  // The timeout monitor that runs from when the beforeunload is started in
  // DispatchBeforeUnload() until either the render process ACKs it with an IPC
  // to OnBeforeUnloadACK(), or until the timeout triggers.
  std::unique_ptr<TimeoutMonitor> beforeunload_timeout_;

  // The delay to use for the beforeunload timeout monitor above.
  base::TimeDelta beforeunload_timeout_delay_;

  // When this frame is asked to execute beforeunload, this maintains a list of
  // frames that need to receive beforeunload ACKs.  This may include this
  // frame and/or its descendant frames.  This excludes frames that don't have
  // beforeunload handlers defined.
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
  // This is passed to RequestNavigationParams in NavigationRequest.
  bool was_discarded_;

  // Indicates whether this RenderFrameHost is in the process of loading a
  // document or not.
  bool is_loading_;

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.  Tracking this
  // allows us to send things like title and state updates to the latest
  // relevant NavigationEntry.
  int nav_entry_id_;

  // Used to swap out or shut down this RFH when the unload event is taking too
  // long to execute, depending on the number of active frames in the
  // SiteInstance.  May be null in tests.
  std::unique_ptr<TimeoutMonitor> swapout_event_monitor_timeout_;

  // GeolocationService which provides Geolocation.
  std::unique_ptr<GeolocationServiceImpl> geolocation_service_;

  // SensorProvider proxy which acts as a gatekeeper to the real SensorProvider.
  std::unique_ptr<SensorProviderProxyImpl> sensor_provider_proxy_;

  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_registry_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;
  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces_;

  std::list<std::unique_ptr<WebBluetoothServiceImpl>> web_bluetooth_services_;

  // The object managing the accessibility tree for this frame.
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // This is nonzero if we sent an accessibility reset to the renderer and
  // we're waiting for an IPC containing this reset token (sequentially
  // assigned) and a complete replacement accessibility tree.
  int accessibility_reset_token_;

  // A count of the number of times we needed to reset accessibility, so
  // we don't keep trying to reset forever.
  int accessibility_reset_count_;

  // The last AXContentTreeData for this frame received from the RenderFrame.
  AXContentTreeData ax_content_tree_data_;

  // The AX tree ID of this frame.
  ui::AXTreeID ax_tree_id_ = ui::AXTreeIDUnknown();

  // The AX tree ID of the embedder, if this is a browser plugin guest.
  ui::AXTreeID browser_plugin_embedder_ax_tree_id_;

  // The mapping from callback id to corresponding callback for pending
  // accessibility tree snapshot calls created by RequestAXTreeSnapshot.
  std::map<int, AXTreeSnapshotCallback> ax_tree_snapshot_callbacks_;

  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
#if defined(OS_ANDROID)
  base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>
      smart_clip_callbacks_;
#endif  // defined(OS_ANDROID)

  // Callback when an event is received, for testing.
  base::Callback<void(RenderFrameHostImpl*, ax::mojom::Event, int)>
      accessibility_testing_callback_;
  // The most recently received accessibility tree - for testing only.
  std::unique_ptr<ui::AXTree> ax_tree_for_testing_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_;

  // Context shared for each mojom::PermissionService instance created for this
  // RFH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // Holder of Mojo connection with ImageDownloader service in RenderFrame.
  content::mojom::ImageDownloaderPtr mojo_image_downloader_;

  // Holder of Mojo connection with FindInPage service in Blink.
  blink::mojom::FindInPageAssociatedPtr find_in_page_;

  // Holds the interface wrapper to the Global Resource Coordinator service.
  std::unique_ptr<resource_coordinator::FrameResourceCoordinator>
      frame_resource_coordinator_;

  // Holds a NavigationRequest when it's about to commit, ie. after
  // OnCrossDocumentCommitProcessed has returned a positive answer for this
  // NavigationRequest but before receiving DidCommitProvisionalLoad. This
  // NavigationRequest is for a cross-document navigation.
  std::unique_ptr<NavigationRequest> navigation_request_;

  // Holds the cross-document NavigationRequests that are waiting to commit,
  // indexed by IDs. These are navigations that have passed ReadyToCommit stage
  // and are waiting for the renderer to send back a matching
  // OnCrossDocumentCommitProcessed.
  std::map<int64_t, std::unique_ptr<NavigationRequest>> navigation_requests_;

  // Holds a same-document NavigationRequest while waiting for the navigation it
  // is tracking to commit.
  std::unique_ptr<NavigationRequest> same_document_navigation_request_;

  // The associated WebUIImpl and its type. They will be set if the current
  // document is from WebUI source. Otherwise they will be null and
  // WebUI::kNoWebUI, respectively.
  std::unique_ptr<WebUIImpl> web_ui_;
  WebUI::TypeID web_ui_type_;

  // The pending WebUIImpl and its type. These values will be used exclusively
  // for same-site navigations to keep a transition of a WebUI in a pending
  // state until the navigation commits.
  std::unique_ptr<WebUIImpl> pending_web_ui_;
  WebUI::TypeID pending_web_ui_type_;

  // If true the associated WebUI should be reused when CommitPendingWebUI is
  // called (no pending instance should be set).
  bool should_reuse_web_ui_;

  // If true, then the RenderFrame has selected text.
  bool has_selection_;

  // If true, then this RenderFrame has one or more audio streams with audible
  // signal. If false, all audio streams are currently silent (or there are no
  // audio streams).
  bool is_audible_;

  // Used for tracking the latest size of the RenderFrame.
  base::Optional<gfx::Size> frame_size_;

  // The Previews state of the last navigation. This is used during history
  // navigation of subframes to ensure that subframes navigate with the same
  // Previews status as the top-level frame.
  PreviewsState last_navigation_previews_state_;

  bool has_committed_any_navigation_ = false;
  mojo::AssociatedBinding<mojom::FrameHost> frame_host_associated_binding_;
  mojom::FramePtr frame_;
  mojom::FrameBindingsControlAssociatedPtr frame_bindings_control_;
  mojom::FrameNavigationControlAssociatedPtr navigation_control_;

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
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap
      non_network_url_loader_factories_;

  // A bundle of subresource loader factories used by this frame.
  // A clone of this bundle is sent to the renderer process.
  scoped_refptr<URLLoaderFactoryBundle> subresource_loader_factories_bundle_;

  // Bitfield for renderer-side state that blocks fast shutdown of the frame.
  blink::WebSuddenTerminationDisablerType
      sudden_termination_disabler_types_enabled_ = 0;

  // Callback for responding when
  // |FrameHostMsg_TextSurroundingSelectionResponse| message comes.
  TextSurroundingSelectionCallback text_surrounding_selection_callback_;

  // We switch between |audio_service_audio_output_stream_factory_| and
  // |in_content_audio_output_stream_factory_| based on
  // features::kAudioServiceAudioStreams status.
  base::Optional<RenderFrameAudioOutputStreamFactory>
      audio_service_audio_output_stream_factory_;
  UniqueAudioOutputStreamFactoryPtr in_content_audio_output_stream_factory_;

  // We switch between |audio_service_audio_input_stream_factory_| and
  // |in_content_audio_input_stream_factory_| based on
  // features::kAudioServiceAudioStreams status.
  base::Optional<RenderFrameAudioInputStreamFactory>
      audio_service_audio_input_stream_factory_;
  UniqueAudioInputStreamFactoryPtr in_content_audio_input_stream_factory_;

  std::unique_ptr<MediaStreamDispatcherHost, BrowserThread::DeleteOnIOThread>
      media_stream_dispatcher_host_;

  // Hosts media::mojom::InterfaceFactory for the RenderFrame and forwards
  // media::mojom::InterfaceFactory calls to the remote "media" service.
  std::unique_ptr<MediaInterfaceProxy> media_interface_proxy_;

  // Hosts blink::mojom::PresentationService for the RenderFrame.
  std::unique_ptr<PresentationServiceImpl> presentation_service_;

  // Hosts blink::mojom::FileSystemManager for the RenderFrame.
  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_;

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
  blink::WebSandboxFlags active_sandbox_flags_;

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
  // or the inital empty document if no real navigation has ever been committed.
  //
  // The InterfaceProvider interface connection is established as follows:
  //
  // 1) For the initial empty document, the call site that creates this
  //    RenderFrameHost is responsible for creating a message pipe, binding its
  //    request end to this instance by calling BindInterfaceProviderRequest(),
  //    and plumbing the client end to the renderer process, and ultimately
  //    supplying it to the RenderFrame synchronously at construction time.
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
  mojo::Binding<service_manager::mojom::InterfaceProvider>
      document_scoped_interface_provider_binding_;

  // Logs interface requests that arrive after the frame has already committed a
  // non-same-document navigation, and has already unbound
  // |document_scoped_interface_provider_binding_| from the interface connection
  // that had been used to service RenderFrame::GetRemoteInterface for the
  // previously active document in the frame.
  std::unique_ptr<DroppedInterfaceRequestLogger>
      dropped_interface_request_logger_;

  // IPC-friendly token that represents this host for AndroidOverlays, if we
  // have created one yet.
  base::Optional<base::UnguessableToken> overlay_routing_token_;

  viz::mojom::InputTargetClient* input_target_client_ = nullptr;
  mojom::FrameInputHandlerPtr frame_input_handler_;

  std::unique_ptr<KeepAliveHandleFactory> keep_alive_handle_factory_;
  base::TimeDelta keep_alive_timeout_;

  base::TimeDelta subframe_unload_timeout_;

  // For observing Network Service connection errors only. Will trigger
  // |OnNetworkServiceConnectionError()| and push updated factories to
  // |RenderFrame|.
  network::mojom::URLLoaderFactoryPtr
      network_service_connection_error_handler_holder_;

  // Set of request-initiator-origins that require a separate URLLoaderFactory
  // (e.g. for handling requests initiated by extension content scripts that
  // require relaxed CORS/CORB rules).
  base::flat_set<url::Origin> initiators_requiring_separate_url_loader_factory_;

  // Holds the renderer generated ID and global request ID for the main frame
  // request.
  std::pair<int, GlobalRequestID> main_frame_request_ids_;

  // If |ResourceLoadComplete()| is called for the main resource before
  // |DidCommitProvisionalLoad()|, the load info is saved here to call
  // |ResourceLoadComplete()| when |DidCommitProvisionalLoad()| is called. This
  // is necessary so the renderer ID can be mapped to the global ID in
  // |DidCommitProvisionalLoad()|. This situation should only happen when an
  // empty document is loaded.
  mojom::ResourceLoadInfoPtr deferred_main_frame_load_info_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
