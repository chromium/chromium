// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/download/mhtml_file_writer.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_delete_intention.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "content/common/navigation_params.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unique_name_helper.h"
#include "content/common/widget.mojom.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/fullscreen_video_element.mojom.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/widget_type.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "content/renderer/content_security_policy_util.h"
#include "content/renderer/frame_blame_context.h"
#include "content/renderer/input/input_target_client_impl.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/media/media_factory.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/routing_token_callback.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_icon_url.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_MACOSX)
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/renderer/pepper/plugin_power_saver_helper.h"
#endif

struct FrameMsg_MixedContentFound_Params;
struct FrameMsg_TextTrackSettings_Params;

namespace blink {
class WebComputedAXTree;
class WebContentDecryptionModule;
class WebElement;
class WebLocalFrame;
class WebMediaStreamDeviceObserver;
class WebSecurityOrigin;
class WebString;
class WebURL;
struct FramePolicy;
struct WebContextMenuData;
struct WebCursorInfo;
struct MediaPlayerAction;
struct WebImeTextSpan;
struct WebScrollIntoViewParams;
}  // namespace blink

namespace gfx {
class Point;
class Range;
}  // namespace gfx

namespace media {
class MediaPermission;
}

namespace service_manager {
class InterfaceProvider;
}

namespace url {
class Origin;
}

namespace content {

class BlinkInterfaceRegistryImpl;
class CompositorDependencies;
class DocumentState;
class ExternalPopupMenu;
class FrameRequestBlocker;
class MediaPermissionDispatcher;
class NavigationClient;
class PepperPluginInstanceImpl;
class RenderAccessibilityImpl;
class RendererPpapiHost;
class RenderFrameObserver;
class RenderViewImpl;
class RenderWidget;
class RenderWidgetFullscreenPepper;
struct CSPViolationParams;
struct CustomContextMenuContext;
struct FrameOwnerProperties;
struct FrameReplicationState;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      blink::mojom::AutoplayConfigurationClient,
      mojom::Frame,
      mojom::FrameNavigationControl,
      mojom::FullscreenVideoElementHandler,
      mojom::FrameBindingsControl,
      mojom::MhtmlFileWriter,
      public blink::WebLocalFrameClient,
      public blink::WebFrameSerializerClient,
      service_manager::mojom::InterfaceProvider {
 public:
  // Creates a new RenderFrame as the main frame of |render_view|.
  static RenderFrameImpl* CreateMainFrame(
      RenderViewImpl* render_view,
      CompositorDependencies* compositor_deps,
      blink::WebFrame* opener,
      mojom::CreateViewParamsPtr* params,
      RenderWidget::ShowCallback show_callback);

  // Creates a new RenderFrame with |routing_id|. If |previous_routing_id| is
  // MSG_ROUTING_NONE, it creates the Blink WebLocalFrame and inserts it into
  // the frame tree after the frame identified by |previous_sibling_routing_id|,
  // or as the first child if |previous_sibling_routing_id| is MSG_ROUTING_NONE.
  // Otherwise, the frame is semi-orphaned until it commits, at which point it
  // replaces the previous object identified by |previous_routing_id|. The
  // previous object can either be a RenderFrame or a RenderFrameProxy.
  // The frame's opener is set to the frame identified by |opener_routing_id|.
  // The frame is created as a child of the RenderFrame identified by
  // |parent_routing_id| or as the top-level frame if
  // the latter is MSG_ROUTING_NONE.
  // |devtools_frame_token| is passed from the browser and corresponds to the
  // owner FrameTreeNode. It can only be used for tagging requests and calls
  // for context frame attribution. It should never be passed back to the
  // browser as a frame identifier in the control flows calls.
  // The |widget_params| is not null if the frame is to be a local root, which
  // means it will own a RenderWidget, in which case the |widget_params| hold
  // the routing id and initialization properties for the RenderWidget.
  //
  // Note: This is called only when RenderFrame is being created in response
  // to IPC message from the browser process. All other frame creation is driven
  // through Blink and Create.
  static void CreateFrame(
      int routing_id,
      service_manager::mojom::InterfaceProviderPtr interface_provider,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      int previous_routing_id,
      int opener_routing_id,
      int parent_routing_id,
      int previous_sibling_routing_id,
      const base::UnguessableToken& devtools_frame_token,
      const FrameReplicationState& replicated_state,
      CompositorDependencies* compositor_deps,
      const mojom::CreateFrameWidgetParams* widget_params,
      const FrameOwnerProperties& frame_owner_properties,
      bool has_committed_real_load);

  // Returns the RenderFrameImpl for the given routing ID.
  static RenderFrameImpl* FromRoutingID(int routing_id);

  // Just like RenderFrame::FromWebFrame but returns the implementation.
  static RenderFrameImpl* FromWebFrame(blink::WebFrame* web_frame);

  // Constructor parameters are bundled into a struct.
  struct CONTENT_EXPORT CreateParams {
    CreateParams(
        RenderViewImpl* render_view,
        int32_t routing_id,
        service_manager::mojom::InterfaceProviderPtr interface_provider,
        mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
            browser_interface_broker,
        const base::UnguessableToken& devtools_frame_token);
    ~CreateParams();

    CreateParams(CreateParams&&);
    CreateParams& operator=(CreateParams&&);

    RenderViewImpl* render_view;
    int32_t routing_id;
    service_manager::mojom::InterfaceProviderPtr interface_provider;
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker;
    base::UnguessableToken devtools_frame_token;
  };

  using CreateRenderFrameImplFunction = RenderFrameImpl* (*)(CreateParams);

  // Web tests override the creation of RenderFrames in order to inject a
  // partial testing fake.
  static void InstallCreateHook(CreateRenderFrameImplFunction create_frame);

  // Looks up and returns the WebFrame corresponding to a given opener frame
  // routing ID.
  static blink::WebFrame* ResolveOpener(int opener_frame_routing_id);

  // Possibly set the kOpenerCrossOrigin and kSandboxNoGesture policy in
  // |download_policy|.
  static void MaybeSetDownloadFramePolicy(
      bool is_opener_navigation,
      const blink::WebURLRequest& request,
      const blink::WebSecurityOrigin& current_origin,
      bool has_download_sandbox_flag,
      bool blocking_downloads_in_sandbox_without_user_activation_enabled,
      bool from_ad,
      NavigationDownloadPolicy* download_policy);

  // Overwrites the given URL to use an HTML5 embed if possible.
  blink::WebURL OverrideFlashEmbedWithHTML(const blink::WebURL& url) override;

  ~RenderFrameImpl() override;

  // Called by RenderWidget when meaningful layout has happened.
  // See RenderFrameObserver::DidMeaningfulLayout declaration for details.
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type);

  // Draw commands have been issued by blink::LayerTreeView.
  void DidCommitAndDrawCompositorFrame();

  // Returns the unique name of the RenderFrame.
  const std::string& unique_name() const { return unique_name_helper_.value(); }

  // TODO(jam): this is a temporary getter until all the code is transitioned
  // to using RenderFrame instead of RenderView.
  RenderViewImpl* render_view() { return render_view_; }

  const blink::WebHistoryItem& current_history_item() {
    return current_history_item_;
  }

  // Returns the RenderWidget associated with this frame.
  RenderWidget* GetLocalRootRenderWidget();

  // This method must be called after the frame has been added to the frame
  // tree. It creates all objects that depend on the frame being at its proper
  // spot.
  void Initialize();

  // Notifications from RenderWidget.
  void WasHidden();
  void WasShown();

  // Start/Stop loading notifications.
  // TODO(nasko): Those are page-level methods at this time and come from
  // WebViewClient. We should move them to be WebLocalFrameClient calls and put
  // logic in the browser side to balance starts/stops.
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidChangeLoadProgress(double load_progress) override;

  ui::AXMode accessibility_mode() { return accessibility_mode_; }

  RenderAccessibilityImpl* render_accessibility() {
    return render_accessibility_;
  }

  // Whether or not the frame is currently swapped into the frame tree.  If
  // this is false, this is a provisional frame which has not committed yet,
  // and which will swap with a proxy when it commits.
  //
  // TODO(https://crbug.com/578349): Remove this once provisional frames are
  // gone, and clean up code that depends on it.
  bool in_frame_tree() { return in_frame_tree_; }

  void HandleWebAccessibilityEvent(const blink::WebAXObject& obj,
                                   ax::mojom::Event event);

  // The focused element changed to |element|. If focus was lost from this
  // frame, |element| will be null.
  void FocusedElementChanged(const blink::WebElement& element);

  // TODO(dmazzoni): the only reason this is here is to plumb it through to
  // RenderAccessibilityImpl. It should use the RenderFrameObserver method, once
  // blink has a separate accessibility tree per frame.
  void FocusedElementChangedForAccessibility(const blink::WebElement& element);

  // A RenderView opened by this RenderFrame needs to be shown.
  void ShowCreatedWindow(bool opened_by_user_gesture,
                         RenderWidget* render_widget_to_show,
                         blink::WebNavigationPolicy policy,
                         const gfx::Rect& initial_rect);

  // Called when this frame's widget is focused.
  void RenderWidgetSetFocus(bool enable);

  // Called when the widget receives a mouse event.
  void RenderWidgetWillHandleMouseEvent();

#if BUILDFLAG(ENABLE_PLUGINS)
  // Notification that a PPAPI plugin has been created.
  void PepperPluginCreated(RendererPpapiHost* host);

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  void PepperDidChangeCursor(PepperPluginInstanceImpl* instance,
                             const blink::WebCursorInfo& cursor);

  // Notifies that |instance| has received a mouse event.
  void PepperDidReceiveMouseEvent(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PepperTextInputTypeChanged(PepperPluginInstanceImpl* instance);
  void PepperCaretPositionChanged(PepperPluginInstanceImpl* instance);

  // Cancels current composition.
  void PepperCancelComposition(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed selection.
  void PepperSelectionChanged(PepperPluginInstanceImpl* instance);

  // Creates a fullscreen container for a pepper plugin instance.
  RenderWidgetFullscreenPepper* CreatePepperFullscreenContainer(
      PepperPluginInstanceImpl* plugin);

  bool IsPepperAcceptingCompositionEvents() const;

  // Notification that the given plugin has crashed.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid);

  // Simulates IME events for testing purpose.
  void SimulateImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebImeTextSpan>& ime_text_spans,
      int selection_start,
      int selection_end);
  void SimulateImeCommitText(
      const base::string16& text,
      const std::vector<blink::WebImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range);

  // TODO(jam): remove these once the IPC handler moves from RenderView to
  // RenderFrame.
  void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebImeTextSpan>& ime_text_spans,
      int selection_start,
      int selection_end);
  void OnImeCommitText(const base::string16& text,
                       const gfx::Range& replacement_range,
                       int relative_cursor_pos);
  void OnImeFinishComposingText(bool keep_selection);

#endif  // BUILDFLAG(ENABLE_PLUGINS)

  void ScriptedPrint(bool user_initiated);

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void DidHideExternalPopupMenu();
#endif

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // RenderFrame implementation:
  RenderView* GetRenderView() override;
  RenderAccessibility* GetRenderAccessibility() override;
  int GetRoutingID() override;
  blink::WebLocalFrame* GetWebFrame() override;
  const WebPreferences& GetWebkitPreferences() override;
  int ShowContextMenu(ContextMenuClient* client,
                      const ContextMenuParams& params) override;
  void CancelContextMenu(int request_id) override;
  void ShowVirtualKeyboard() override;
  blink::WebPlugin* CreatePlugin(
      const WebPluginInfo& info,
      const blink::WebPluginParams& params,
      std::unique_ptr<PluginInstanceThrottler> throttler) override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  bool IsMainFrame() override;
  bool IsHidden() override;
  void BindLocalInterface(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;
  blink::AssociatedInterfaceRegistry* GetAssociatedInterfaceRegistry() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
#if BUILDFLAG(ENABLE_PLUGINS)
  void RegisterPeripheralPlugin(const url::Origin& content_origin,
                                base::OnceClosure unthrottle_callback) override;
  RenderFrame::PeripheralContentStatus GetPeripheralContentStatus(
      const url::Origin& main_frame_origin,
      const url::Origin& content_origin,
      const gfx::Size& unobscured_size,
      RecordPeripheralDecision record_decision) override;
  void WhitelistContentOrigin(const url::Origin& content_origin) override;
  void PluginDidStartLoading() override;
  void PluginDidStopLoading() override;
#endif
  bool IsFTPDirectoryListing() override;
  void AttachGuest(int element_instance_id) override;
  void DetachGuest(int element_instance_id) override;
  void SetSelectedText(const base::string16& selection_text,
                       size_t offset,
                       const gfx::Range& range) override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  PreviewsState GetPreviewsState() override;
  bool IsPasting() override;
  bool IsBrowserSideNavigationPending() override;
  void LoadHTMLString(const std::string& html,
                      const GURL& base_url,
                      const std::string& text_encoding,
                      const GURL& unreachable_url,
                      bool replace_current_item) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      blink::TaskType task_type) override;
  int GetEnabledBindings() override;
  void SetAccessibilityModeForTest(ui::AXMode new_mode) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  const RenderFrameMediaPlaybackOptions& GetRenderFrameMediaPlaybackOptions()
      override;
  void SetRenderFrameMediaPlaybackOptions(
      const RenderFrameMediaPlaybackOptions& opts) override;
  void UpdateAllLifecyclePhasesAndCompositeForTesting() override;
  void SetAllowsCrossBrowsingInstanceFrameLookup() override;
  gfx::RectF ElementBoundsInWindow(const blink::WebElement& element) override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  float GetDeviceScaleFactor() override;

  // blink::mojom::AutoplayConfigurationClient implementation:
  void AddAutoplayFlags(const url::Origin& origin,
                        const int32_t flags) override;

  // mojom::Frame implementation:
  void GetInterfaceProvider(
      service_manager::mojom::InterfaceProviderRequest request) override;
  void GetCanonicalUrlForSharing(
      GetCanonicalUrlForSharingCallback callback) override;
  void BlockRequests() override;
  void ResumeBlockedRequests() override;
  void CancelBlockedRequests() override;
  void SetLifecycleState(blink::mojom::FrameLifecycleState state) override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;

#if defined(OS_ANDROID)
  void ExtractSmartClipData(
      const gfx::Rect& rect,
      const ExtractSmartClipDataCallback callback) override;
#endif

  // mojom::FrameBindingsControl implementation:
  void AllowBindings(int32_t enabled_bindings_flags) override;
  void EnableMojoJsBindings() override;

  // mojom::FrameNavigationControl implementation:
  void PostMessageEvent(int32_t source_routing_id,
                        const base::string16& source_origin,
                        const base::string16& target_origin,
                        blink::TransferableMessage message) override;
  void ForwardMessageFromHost(
      blink::TransferableMessage message,
      const url::Origin& source_origin,
      const base::Optional<url::Origin>& target_origin) override;

  // mojom::FrameNavigationControl implementation:
  void CommitNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      mojom::FrameNavigationControl::CommitNavigationCallback commit_callback)
      override;

  // This is the version to be used with PerNavigationMojoInterface enabled.
  // It essentially works the same way, except the navigation callback is
  // the one from NavigationClient mojo interface.
  void CommitPerNavigationMojoInterfaceNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      mojom::NavigationClient::CommitNavigationCallback
          per_navigation_mojo_interface_callback);

  void CommitFailedNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      mojom::NavigationClient::CommitFailedNavigationCallback
          per_navigation_mojo_interface_callback);

  void CommitSameDocumentNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      CommitSameDocumentNavigationCallback callback) override;
  void HandleRendererDebugURL(const GURL& url) override;
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories) override;
  void BindDevToolsAgent(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver)
      override;

  void JavaScriptExecuteRequest(
      const base::string16& javascript,
      bool wants_result,
      JavaScriptExecuteRequestCallback callback) override;
  void JavaScriptExecuteRequestForTests(
      const base::string16& javascript,
      bool wants_result,
      bool has_user_gesture,
      int32_t world_id,
      JavaScriptExecuteRequestForTestsCallback callback) override;
  void JavaScriptExecuteRequestInIsolatedWorld(
      const base::string16& javascript,
      bool wants_result,
      int32_t world_id,
      JavaScriptExecuteRequestInIsolatedWorldCallback callback) override;
  void OnPortalActivated(
      const base::UnguessableToken& portal_token,
      mojo::PendingAssociatedRemote<blink::mojom::Portal> portal,
      mojo::PendingAssociatedReceiver<blink::mojom::PortalClient> portal_client,
      blink::TransferableMessage data,
      OnPortalActivatedCallback callback) override;
  void ReportContentSecurityPolicyViolation(
      const content::CSPViolationParams& violation_params) override;

  // mojom::FullscreenVideoElementHandler implementation:
  void RequestFullscreenVideoElement() override;

  // mojom::MhtmlFileWriter implementation:
  void SerializeAsMHTML(const mojom::SerializeAsMHTMLParamsPtr params,
                        SerializeAsMHTMLCallback callback) override;

  // blink::WebLocalFrameClient implementation:
  void BindToFrame(blink::WebNavigationControl* frame) override;
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  blink::WebMediaPlayer* CreateMediaPlayer(
      const blink::WebMediaPlayerSource& source,
      blink::WebMediaPlayerClient* client,
      blink::MediaInspectorContext* inspector_context,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      const blink::WebString& sink_id) override;
  std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient() override;
  scoped_refptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext()
      override;
  scoped_refptr<blink::WebWorkerFetchContext>
  CreateWorkerFetchContextForPlzDedicatedWorker(
      blink::WebDedicatedWorkerHostFactoryClient* factory_client) override;
  blink::WebExternalPopupMenu* CreateExternalPopupMenu(
      const blink::WebPopupMenuInfo& popup_menu_info,
      blink::WebExternalPopupMenuClient* popup_menu_client) override;
  blink::BlameContext* GetFrameBlameContext() override;
  std::unique_ptr<blink::WebServiceWorkerProvider> CreateServiceWorkerProvider()
      override;
  service_manager::InterfaceProvider* GetInterfaceProvider() override;
  blink::AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces()
      override;
  void DidAccessInitialDocument() override;
  blink::WebLocalFrame* CreateChildFrame(
      blink::WebLocalFrame* parent,
      blink::WebTreeScopeType scope,
      const blink::WebString& name,
      const blink::WebString& fallback_name,
      const blink::FramePolicy& frame_policy,
      const blink::WebFrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType frame_owner_element_type) override;
  std::pair<blink::WebRemoteFrame*, base::UnguessableToken> CreatePortal(
      mojo::ScopedInterfaceEndpointHandle portal_endpoint,
      mojo::ScopedInterfaceEndpointHandle client_endpoint,
      const blink::WebElement& portal_element) override;
  blink::WebRemoteFrame* AdoptPortal(
      const base::UnguessableToken& portal_token,
      const blink::WebElement& portal_element) override;
  blink::WebFrame* FindFrame(const blink::WebString& name) override;
  void DidChangeOpener(blink::WebFrame* frame) override;
  void FrameDetached(DetachType type) override;
  void DidChangeName(const blink::WebString& name) override;
  void DidChangeFramePolicy(blink::WebFrame* child_frame,
                            const blink::FramePolicy& frame_policy) override;
  void DidSetFramePolicyHeaders(
      blink::WebSandboxFlags flags,
      const blink::ParsedFeaturePolicy& parsed_header) override;
  void DidAddContentSecurityPolicies(
      const blink::WebVector<blink::WebContentSecurityPolicy>&) override;
  void DidChangeFrameOwnerProperties(
      blink::WebFrame* child_frame,
      const blink::WebFrameOwnerProperties& frame_owner_properties) override;
  void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
      override;
  void UpdateUserActivationState(
      blink::UserActivationUpdateType update_type) override;
  void SetHasReceivedUserGestureBeforeNavigation(bool value) override;
  void SetMouseCapture(bool capture) override;
  bool ShouldReportDetailedMessageForSource(
      const blink::WebString& source) override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void DownloadURL(const blink::WebURLRequest& request,
                   network::mojom::RedirectMode cross_origin_redirect_behavior,
                   mojo::ScopedMessagePipeHandle blob_url_token) override;
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void DidCreateDocumentLoader(
      blink::WebDocumentLoader* document_loader) override;
  void DidStartProvisionalLoad(
      blink::WebDocumentLoader* document_loader) override;
  void DidCommitProvisionalLoad(
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker) override;
  void DidCreateNewDocument() override;
  void DidClearWindowObject() override;
  void DidCreateDocumentElement() override;
  void RunScriptsAtDocumentElementAvailable() override;
  void DidReceiveTitle(const blink::WebString& title,
                       blink::WebTextDirection direction) override;
  void DidChangeIcon(blink::WebIconURL::Type icon_type) override;
  void DidFinishDocumentLoad() override;
  void RunScriptsAtDocumentReady(bool document_is_empty) override;
  void RunScriptsAtDocumentIdle() override;
  void DidHandleOnloadEvents() override;
  void DidFailLoad(const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override;
  void DidFinishLoad() override;
  void DidFinishSameDocumentNavigation(const blink::WebHistoryItem& item,
                                       blink::WebHistoryCommitType commit_type,
                                       bool content_initiated) override;
  void DidUpdateCurrentHistoryItem() override;
  void ForwardResourceTimingToParent(
      const blink::WebResourceTimingInfo& info) override;
  void DispatchLoad() override;
  void DidBlockNavigation(const blink::WebURL& blocked_url,
                          const blink::WebURL& initiator_url,
                          blink::NavigationBlockedReason reason) override;
  void NavigateBackForwardSoon(int offset, bool has_user_gesture) override;
  base::UnguessableToken GetDevToolsFrameToken() override;
  void RenderFallbackContentInParentProcess() override;
  void AbortClientNavigation() override;
  void DidChangeSelection(bool is_empty_selection) override;
  bool HandleCurrentKeyboardEvent() override;
  void RunModalAlertDialog(const blink::WebString& message) override;
  bool RunModalConfirmDialog(const blink::WebString& message) override;
  bool RunModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString* actual_value) override;
  bool RunModalBeforeUnloadDialog(bool is_reload) override;
  void ShowContextMenu(const blink::WebContextMenuData& data) override;
  void SaveImageFromDataURL(const blink::WebString& data_url) override;
  void FrameRectsChanged(const blink::WebRect& frame_rect) override;
  void WillSendRequest(blink::WebURLRequest& request) override;
  void DidLoadResourceFromMemoryCache(
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response) override;
  void DidRunInsecureContent(const blink::WebSecurityOrigin& origin,
                             const blink::WebURL& target) override;
  void DidDisplayContentWithCertificateErrors() override;
  void DidRunContentWithCertificateErrors() override;
  void DidChangePerformanceTiming() override;
  void DidChangeCpuTiming(base::TimeDelta time) override;
  void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior) override;
  void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature) override;
  void DidObserveNewCssPropertyUsage(blink::mojom::CSSSampleId css_property,
                                     bool is_animated) override;
  void DidObserveLayoutShift(double score, bool after_input_or_scroll) override;
  void DidObserveLazyLoadBehavior(
      blink::WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) override;
  bool ShouldTrackUseCounter(const blink::WebURL& url) override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override;
  void DidChangeScrollOffset() override;
  blink::WebMediaStreamDeviceObserver* MediaStreamDeviceObserver() override;
  blink::WebEncryptedMediaClient* EncryptedMediaClient() override;
  blink::WebString UserAgentOverride() override;
  blink::WebString DoNotTrackValue() override;
  mojom::RendererAudioInputStreamFactory* GetAudioInputStreamFactory();
  bool ShouldBlockWebGL() override;
  bool AllowContentInitiatedDataUrlNavigations(
      const blink::WebURL& url) override;
  void PostAccessibilityEvent(const blink::WebAXObject& obj,
                              ax::mojom::Event event,
                              ax::mojom::EventFrom event_from) override;
  void MarkWebAXObjectDirty(const blink::WebAXObject& obj,
                            bool subtree) override;
  void HandleAccessibilityFindInPageResult(int identifier,
                                           int match_index,
                                           const blink::WebNode& start_node,
                                           int start_offset,
                                           const blink::WebNode& end_node,
                                           int end_offset) override;
  void HandleAccessibilityFindInPageTermination() override;
  void SuddenTerminationDisablerChanged(
      bool present,
      blink::SuddenTerminationDisablerType disabler_type) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      blink::WebSetSinkIdCompleteCallback callback) override;
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory() override;
  void DraggableRegionsChanged() override;
  // |rect_to_scroll| is with respect to this frame's origin. |rect_to_scroll|
  // will later be converted to this frame's parent frame origin before being
  // continuing recursive scrolling in the parent frame's process.
  void ScrollRectToVisibleInParentFrame(
      const blink::WebRect& rect_to_scroll,
      const blink::WebScrollIntoViewParams& params) override;
  void BubbleLogicalScrollInParentFrame(
      blink::WebScrollDirection direction,
      ui::input_types::ScrollGranularity granularity) override;
  blink::BrowserInterfaceBrokerProxy* GetBrowserInterfaceBroker() override;

  // WebFrameSerializerClient implementation:
  void DidSerializeDataForFrame(
      const blink::WebVector<char>& data,
      blink::WebFrameSerializerClient::FrameSerializationStatus status)
      override;

  // Binds to the fullscreen service in the browser.
  void BindFullscreen(
      mojo::PendingAssociatedReceiver<mojom::FullscreenVideoElementHandler>
          receiver);

  // Binds to the MHTML file generation service in the browser.
  void BindMhtmlFileWriter(
      mojo::PendingAssociatedReceiver<mojom::MhtmlFileWriter> receiver);

  // Binds to the autoplay configuration service in the browser.
  void BindAutoplayConfiguration(
      mojo::PendingAssociatedReceiver<blink::mojom::AutoplayConfigurationClient>
          receiver);

  // Binds to the FrameHost in the browser.
  void BindFrame(mojo::PendingReceiver<mojom::Frame> receiver);

  // Virtual so that a TestRenderFrame can mock out the interface.
  virtual mojom::FrameHost* GetFrameHost();

  void BindFrameBindingsControl(
      mojo::PendingAssociatedReceiver<mojom::FrameBindingsControl> receiver);
  void BindFrameNavigationControl(
      mojo::PendingAssociatedReceiver<mojom::FrameNavigationControl> receiver);
  // Only used when PerNavigationMojoInterface is enabled.
  void BindNavigationClient(
      mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver);

  media::MediaPermission* GetMediaPermission();

  // Proxies the call to set the zoom level over to the RenderViewImpl and
  // returns its result. Meant to be called by the |render_widget_| in order to
  // get access to the RenderViewImpl.
  bool SetZoomLevelOnRenderView(double zoom_level);
  // Proxies the call to set the prefer compositing flag over to the
  // RenderViewImpl. Meant to be called by the |render_widget_| in order to get
  // access to the RenderViewImpl.
  void SetPreferCompositingToLCDTextEnabledOnRenderView(bool prefer);
  // Proxies the call to set the device scale factor over to the RenderViewImpl.
  // Meant to be called by the |render_widget_| in order to get access to the
  // RenderViewImpl.
  void SetDeviceScaleFactorOnRenderView(bool use_zoom_for_dsf,
                                        float device_scale_factor);
  // Proxies the call to set the visible viewport size over to the
  // RenderViewImpl. Meant to be called by the |render_widget_| in order to get
  // access to the RenderViewImpl.
  void SetVisibleViewportSizeOnRenderView(
      const gfx::Size& visible_viewport_size);

  // Sends the current frame's navigation state to the browser.
  void SendUpdateState();

  // Creates a MojoBindingsController if Mojo bindings have been enabled for
  // this frame. For WebUI, this allows the page to communicate with the browser
  // process; for layout tests, this allows the test to mock out services at
  // the Mojo IPC layer.
  void MaybeEnableMojoBindings();

  void NotifyObserversOfFailedProvisionalLoad();

  bool handling_select_range() const { return handling_select_range_; }

  void set_is_pasting(bool value) { is_pasting_ = value; }

  void set_handling_select_range(bool value) { handling_select_range_ = value; }

  // Plugin-related functions --------------------------------------------------

#if BUILDFLAG(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin() {
    return focused_pepper_plugin_;
  }
  PepperPluginInstanceImpl* pepper_last_mouse_event_target() {
    return pepper_last_mouse_event_target_;
  }
  void set_pepper_last_mouse_event_target(PepperPluginInstanceImpl* plugin) {
    pepper_last_mouse_event_target_ = plugin;
  }

  // Indicates that the given instance has been created.
  void PepperInstanceCreated(PepperPluginInstanceImpl* instance);

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  void PepperInstanceDeleted(PepperPluginInstanceImpl* instance);

  // Notification that the given plugin is focused or unfocused.
  void PepperFocusChanged(PepperPluginInstanceImpl* instance, bool focused);

  void PepperStartsPlayback(PepperPluginInstanceImpl* instance);
  void PepperStopsPlayback(PepperPluginInstanceImpl* instance);
  void OnSetPepperVolume(int32_t pp_instance, double volume);
#endif  // ENABLE_PLUGINS

  const blink::mojom::RendererPreferences& GetRendererPreferences() const;

#if defined(OS_MACOSX)
  void OnCopyToFindPboard();
  void OnClipboardHostError();
#endif

  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed.
  // TODO(varunjain): delete this method once we figure out how to keep
  // selection handles in sync with the webpage.
  void SyncSelectionIfRequired();

  void ScrollFocusedEditableElementIntoRect(const gfx::Rect& rect);
  void ResetHasScrolledFocusedEditableIntoView();

  // Called to notify a frame that it called |window.focus()| on a different
  // frame.
  void FrameDidCallFocus();

  // Called when an ongoing renderer-initiated navigation was dropped by the
  // browser.
  void OnDroppedNavigation();

  void DidStartResponse(const url::Origin& origin_of_final_response_url,
                        int request_id,
                        network::mojom::URLResponseHeadPtr response_head,
                        content::ResourceType resource_type,
                        PreviewsState previews_state);
  void DidCompleteResponse(int request_id,
                           const network::URLLoaderCompletionStatus& status);
  void DidCancelResponse(int request_id);
  void DidReceiveTransferSizeUpdate(int request_id, int received_data_length);

  void TransferUserActivationFrom(blink::WebLocalFrame* source_frame) override;

  // Used in tests to install a fake WebURLLoaderFactory via
  // RenderViewTest::CreateFakeWebURLLoaderFactory().
  void SetWebURLLoaderFactoryOverrideForTest(
      std::unique_ptr<blink::WebURLLoaderFactoryForTest> factory);

 protected:
  explicit RenderFrameImpl(CreateParams params);

  bool IsLocalRoot() const;
  const RenderFrameImpl* GetLocalRoot() const;

 private:
  friend class RenderFrameImplTest;
  friend class RenderFrameObserver;
  friend class RenderAccessibilityImplTest;
  friend class TestRenderFrame;

  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuDisplayNoneTest, SelectItem);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuRemoveTest, RemoveFrameOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuRemoveTest, RemoveOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, NormalCase);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, ShowPopupThenNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderAccessibilityImplTest,
                           AccessibilityMessagesQueueWhileSwappedOut);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplTest, LocalChildFrameWasShown);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplTest, ZoomLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplTest,
                           TestOverlayRoutingTokenSendsLater);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplTest,
                           TestOverlayRoutingTokenSendsNow);

  // A wrapper class used as the callback for JavaScript executed
  // in an isolated world.
  class JavaScriptIsolatedWorldRequest
      : public blink::WebScriptExecutionCallback {
   public:
    JavaScriptIsolatedWorldRequest(
        base::WeakPtr<RenderFrameImpl> render_frame_impl,
        bool wants_result,
        JavaScriptExecuteRequestInIsolatedWorldCallback callback);
    void Completed(
        const blink::WebVector<v8::Local<v8::Value>>& result) override;

   private:
    ~JavaScriptIsolatedWorldRequest() override;

    base::WeakPtr<RenderFrameImpl> render_frame_impl_;
    bool wants_result_;
    JavaScriptExecuteRequestInIsolatedWorldCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(JavaScriptIsolatedWorldRequest);
  };

  // Similar to base::AutoReset, but skips restoration of the original value if
  // |this| is already destroyed.
  template <typename T>
  class AutoResetMember {
   public:
    AutoResetMember(RenderFrameImpl* frame,
                    T RenderFrameImpl::*member,
                    T new_value)
        : weak_frame_(frame->weak_factory_.GetWeakPtr()),
          scoped_variable_(&(frame->*member)),
          original_value_(*scoped_variable_) {
      *scoped_variable_ = new_value;
    }

    ~AutoResetMember() {
      if (weak_frame_)
        *scoped_variable_ = original_value_;
    }

   private:
    base::WeakPtr<RenderFrameImpl> weak_frame_;
    T* scoped_variable_;
    T original_value_;
  };

  class FrameURLLoaderFactory;

  // Creates a new RenderFrame. |render_view| is the RenderView object that this
  // frame belongs to, |interface_provider| is the RenderFrameHost's
  // InterfaceProvider through which services are exposed to the RenderFrame,
  // and |browser_interface_broker| is the RenderFrameHost's
  // BrowserInterfaceBroker through which services are exposed to the
  // RenderFrame.
  static RenderFrameImpl* Create(
      RenderViewImpl* render_view,
      int32_t routing_id,
      service_manager::mojom::InterfaceProviderPtr interface_provider,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      const base::UnguessableToken& devtools_frame_token);

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  // Swaps the current frame into the frame tree, replacing the
  // RenderFrameProxy it is associated with.  Return value indicates whether
  // the swap operation succeeded.  This should only be used for provisional
  // frames associated with a proxy, while the proxy is still in the frame
  // tree. If the associated proxy has been detached before this is called,
  // this returns false and aborts the swap.
  bool SwapIn();

  // Returns the RenderWidget associated with the main frame.
  // TODO(ajwong): This method should go away when cross-frame property setting
  // events moves into RenderWidget.
  RenderWidget* GetMainFrameRenderWidget();

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnBeforeUnload(bool is_reload);
  void OnSwapIn();
  void OnSwapOut(int proxy_routing_id,
                 bool is_loading,
                 const FrameReplicationState& replicated_frame_state);
  void OnDeleteFrame(FrameDeleteIntention intent);
  void OnStop();
  void OnCollapse(bool collapse);
  void OnShowContextMenu(const gfx::Point& location);
  void OnContextMenuClosed(const CustomContextMenuContext& custom_context);
  void OnCustomContextMenuAction(const CustomContextMenuContext& custom_context,
                                 unsigned action);
  void OnMoveCaret(const gfx::Point& point);
  void OnScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);
  void OnSelectRange(const gfx::Point& base, const gfx::Point& extent);
  void OnCopyImageAt(int x, int y);
  void OnSaveImageAt(int x, int y);
  void OnVisualStateRequest(uint64_t key);
  // TODO(https://crbug.com/995428): Deprecated.
  void OnReload();
  void OnSetAccessibilityMode(ui::AXMode new_mode);
  void OnSnapshotAccessibilityTree(int callback_id, ui::AXMode ax_mode);
  void OnUpdateOpener(int opener_routing_id);
  void OnDidUpdateFramePolicy(const blink::FramePolicy& frame_policy);
  void OnSetFrameOwnerProperties(
      const FrameOwnerProperties& frame_owner_properties);
  void OnAdvanceFocus(blink::WebFocusType type, int32_t source_routing_id);
  void OnAdvanceFocusInForm(blink::WebFocusType focus_type);
  void OnSetFocusedFrame();
  void OnTextTrackSettingsChanged(
      const FrameMsg_TextTrackSettings_Params& params);
  void OnGetSavableResourceLinks();
  void OnGetSerializedHtmlWithLocalLinks(
      const std::map<GURL, base::FilePath>& url_to_local_path,
      const std::map<int, base::FilePath>& frame_routing_id_to_local_path,
      bool save_with_empty_url);
  void OnEnableViewSourceMode();
  void OnSuppressFurtherDialogs();
  void OnClearFocusedElement();
  void OnBlinkFeatureUsageReport(
      const std::set<blink::mojom::WebFeature>& features);
  void OnMixedContentFound(const FrameMsg_MixedContentFound_Params& params);
  void OnSetOverlayRoutingToken(const base::UnguessableToken& token);
  void OnMediaPlayerActionAt(const gfx::PointF&,
                             const blink::MediaPlayerAction&);
  void OnRenderFallbackContent() const;

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  void OnSelectPopupMenuItem(int selected_index);
#else
  void OnSelectPopupMenuItems(bool canceled,
                              const std::vector<int>& selected_indices);
#endif
#endif

  // Callback scheduled from SerializeAsMHTML for when writing serialized
  // MHTML to the handle has been completed in the file thread.
  void OnWriteMHTMLComplete(
      SerializeAsMHTMLCallback callback,
      std::unordered_set<std::string> serialized_resources_uri_digests,
      base::TimeDelta main_thread_use_time,
      mojom::MhtmlSaveStatus save_status);

  // Requests that the browser process navigates to |url|.
  void OpenURL(std::unique_ptr<blink::WebNavigationInfo> info);

  // Returns a ChildURLLoaderFactoryBundle which can be used to request
  // subresources for this frame.
  // For frames with committed navigations, this bundle is created with the
  // factories provided by the browser at navigation time. For any other frames
  // (i.e. frames on the initial about:blank Document), the bundle returned here
  // is lazily cloned from the parent or opener's own bundle.
  ChildURLLoaderFactoryBundle* GetLoaderFactoryBundle();

  // Clones and returns the creator's (parent's or opener's)
  // ChildURLLoaderFactoryBundle.
  scoped_refptr<ChildURLLoaderFactoryBundle>
  GetLoaderFactoryBundleFromCreator();

  scoped_refptr<ChildURLLoaderFactoryBundle> CreateLoaderFactoryBundle(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo> info,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory);
  void SetLoaderFactoryBundle(
      scoped_refptr<ChildURLLoaderFactoryBundle> loader_factories);

  // Update current main frame's encoding and send it to browser window.
  // Since we want to let users see the right encoding info from menu
  // before finishing loading, we call the UpdateEncoding in
  // a) function:DidCommitLoadForFrame. When this function is called,
  // that means we have got first data. In here we try to get encoding
  // of page if it has been specified in http header.
  // b) function:DidReceiveTitle. When this function is called,
  // that means we have got specified title. Because in most of webpages,
  // title tags will follow meta tags. In here we try to get encoding of
  // page if it has been specified in meta tag.
  // c) function:DidFinishDocumentLoadForFrame. When this function is
  // called, that means we have got whole html page. In here we should
  // finally get right encoding of page.
  void UpdateEncoding(blink::WebFrame* frame, const std::string& encoding_name);

  bool RunJavaScriptDialog(JavaScriptDialogType type,
                           const base::string16& message,
                           const base::string16& default_value,
                           base::string16* result);

  base::Value GetJavaScriptExecutionResult(v8::Local<v8::Value> result);

  void InitializeMediaStreamDeviceObserver();

  // Does preparation for the navigation to |url|.
  void PrepareRenderViewForNavigation(
      const GURL& url,
      const mojom::CommitNavigationParams& commit_params);

  // Sends a FrameHostMsg_BeginNavigation to the browser
  void BeginNavigationInternal(std::unique_ptr<blink::WebNavigationInfo> info,
                               bool is_history_navigation_in_new_child_frame);

  // Used to load the initial empty document. This one is special, since it
  // isn't the result of a navigation.
  //
  // TODO(arthursonzogni): This function is also used for renderer initiated
  // navigations to about:blank pages. The browser initiated ones uses the
  // normal code path in the browser process. Stop maintaining two code path by
  // removing this one.
  void CommitSyncNavigation(std::unique_ptr<blink::WebNavigationInfo> info);

  // Commit navigation with |navigation_params| prepared.
  void CommitNavigationWithParams(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      std::unique_ptr<DocumentState> document_state,
      std::unique_ptr<blink::WebNavigationParams> navigation_params);

  // We can ignore renderer-initiated navigations which have been canceled
  // in the renderer, but browser was not aware yet at the moment of issuing
  // a CommitNavigation call.
  bool ShouldIgnoreCommitNavigation(
      const mojom::CommitNavigationParams& commit_params);

  // Decodes a data url for navigation commit.
  void DecodeDataURL(const mojom::CommonNavigationParams& common_params,
                     const mojom::CommitNavigationParams& commit_params,
                     std::string* mime_type,
                     std::string* charset,
                     std::string* data,
                     GURL* base_url);

  bool ShouldDisplayErrorPageForFailedLoad(int error_code,
                                           const GURL& unreachable_url);

  // |transition_type| corresponds to the document which triggered this request.
  void WillSendRequestInternal(blink::WebURLRequest& request,
                               ResourceType resource_type,
                               ui::PageTransition transition_type);

  // Returns the URL being loaded by the |frame_|'s request.
  GURL GetLoadingUrl() const;

#if BUILDFLAG(ENABLE_PLUGINS)
  void HandlePepperImeCommit(const base::string16& text);
#endif  // ENABLE_PLUGINS

  void RegisterMojoInterfaces();

  void InitializeBlameContext(RenderFrameImpl* parent_frame);

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Send |callback| our AndroidOverlay routing token when it arrives.  We may
  // call |callback| before returning.
  void RequestOverlayRoutingToken(media::RoutingTokenCallback callback);

  // Ask the host to send our AndroidOverlay routing token to us.
  void RequestOverlayRoutingTokenFromHost();

  void SendUpdateFaviconURL();

  void BindWidget(mojo::PendingReceiver<mojom::Widget> receiver);

  void ShowDeferredContextMenu(const ContextMenuParams& params);

  // Build DidCommitProvisionalLoad_Params based on the frame internal state.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  MakeDidCommitProvisionalLoadParams(blink::WebHistoryCommitType commit_type,
                                     ui::PageTransition transition);

  // Updates the navigation history depending on the passed parameters.
  // This could result either in the creation of a new entry or a modification
  // of the current entry or nothing. If a new entry was created,
  // returns true, false otherwise.
  void UpdateNavigationHistory(const blink::WebHistoryItem& item,
                               blink::WebHistoryCommitType commit_type);

  // Notify render_view_ observers that a commit happened.
  void NotifyObserversOfNavigationCommit(bool is_same_document,
                                         ui::PageTransition transition);

  // Updates the internal state following a navigation commit. This should be
  // called before notifying the FrameHost of the commit.
  void UpdateStateForCommit(const blink::WebHistoryItem& item,
                            blink::WebHistoryCommitType commit_type,
                            ui::PageTransition transition);

  // Internal function used by same document navigation as well as cross
  // document navigation that updates the state of the RenderFrameImpl and sends
  // a commit message to the browser process.
  void DidCommitNavigationInternal(
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type,
      bool was_within_same_document,
      ui::PageTransition transition,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

  blink::WebComputedAXTree* GetOrCreateWebComputedAXTree() override;

  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() override;
  bool IsPluginHandledExternally(
      const blink::WebElement& plugin_element,
      const blink::WebURL& url,
      const blink::WebString& suggested_mime_type) override;
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate) override;

  void UpdateSubresourceFactory(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo> info) override;

  // Updates the state of this frame when asked to commit a navigation.
  void PrepareFrameForCommit(
      const GURL& url,
      const mojom::CommitNavigationParams& commit_params);

  // Updates the state when asked to commit a history navigation.  Sets
  // |item_for_history_navigation| and |load_type| to the appropriate values for
  // commit.
  //
  // The function will also return whether to proceed with the commit of a
  // history navigation or not.  This can return false when the state of the
  // history in the browser process goes out of sync with the renderer process.
  // This can happen in the following scenario:
  //   * this RenderFrame has a document with URL foo, which does a push state
  //     to foo#bar.
  //   * the user starts a navigation to foo/bar.
  //   * the browser process asks this renderer process to commit the navigation
  //     to foo/bar.
  //   * the browser process starts a navigation back to foo, which it
  //     considers same-document since the navigation to foo/bar hasn't
  //     committed yet. It asks the RenderFrame to commit the same-document
  //     navigation to foo#bar.
  //   * by the time the RenderFrame receives the call to commit the
  //     same-document back navigation, the navigation to foo/bar has committed.
  //     A back navigation to foo is no longer same-document with the current
  //     document of the RenderFrame (foo/bar). Therefore, the navigation cannot
  //     be committed as a same-document navigation.
  // When this happens, the navigation will be sent back to the browser process
  // so that it can be performed in cross-document fashion.
  blink::mojom::CommitResult PrepareForHistoryNavigationCommit(
      const mojom::CommonNavigationParams& common_params,
      const mojom::CommitNavigationParams& commit_params,
      blink::WebHistoryItem* item_for_history_navigation,
      blink::WebFrameLoadType* load_type);

  // Whether url download should be throttled.
  bool ShouldThrottleDownload();

  // These functions avoid duplication between Commit*Navigation and
  // Commit*PerNavigationMojoInterfaceNavigation functions.
  void CommitNavigationInternal(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      mojom::FrameNavigationControl::CommitNavigationCallback callback,
      mojom::NavigationClient::CommitNavigationCallback
          per_navigation_mojo_interface_callback);

  // Ignores the navigation commit and stop its processing in the RenderFrame.
  // This will drop the NavigationRequest in the RenderFrameHost.
  // Note: This is only meant to be used before building the DocumentState.
  // Commit abort and navigation end is handled by it afterwards.
  void AbortCommitNavigation(
      mojom::FrameNavigationControl::CommitNavigationCallback callback,
      blink::mojom::CommitResult reason);

  // Implements AddMessageToConsole().
  void AddMessageToConsoleImpl(blink::mojom::ConsoleMessageLevel level,
                               const std::string& message,
                               bool discard_duplicates);

  // Stores the WebLocalFrame we are associated with.  This is null from the
  // constructor until BindToFrame() is called, and it is null after
  // FrameDetached() is called until destruction (which is asynchronous in the
  // case of the main frame, but not subframes).
  blink::WebNavigationControl* frame_ = nullptr;

  // Boolean value indicating whether this RenderFrameImpl object is for the
  // main frame or not. It remains accurate during destruction, even when
  // |frame_| has been invalidated.
  bool is_main_frame_;

  class UniqueNameFrameAdapter : public UniqueNameHelper::FrameAdapter {
   public:
    explicit UniqueNameFrameAdapter(RenderFrameImpl* render_frame);
    ~UniqueNameFrameAdapter() override;

    // FrameAdapter overrides:
    bool IsMainFrame() const override;
    bool IsCandidateUnique(base::StringPiece name) const override;
    int GetSiblingCount() const override;
    int GetChildCount() const override;
    std::vector<base::StringPiece> CollectAncestorNames(
        BeginPoint begin_point,
        bool (*should_stop)(base::StringPiece)) const override;
    std::vector<int> GetFramePosition(BeginPoint begin_point) const override;

   private:
    blink::WebLocalFrame* GetWebFrame() const;

    RenderFrameImpl* render_frame_;
  };
  UniqueNameFrameAdapter unique_name_frame_adapter_;
  UniqueNameHelper unique_name_helper_;

  // Indicates whether the frame has been inserted into the frame tree yet or
  // not.
  //
  // When a frame is created by the browser process, it is for a pending
  // navigation. In this case, it is not immediately attached to the frame tree
  // if there is a RenderFrameProxy for the same frame. It is inserted into the
  // frame tree at the time the pending navigation commits.
  // Frames added by the parent document are created from the renderer process
  // and are immediately inserted in the frame tree.
  // TODO(dcheng): Remove this once we have FrameTreeHandle and can use the
  // Blink Web* layer to check for provisional frames.
  bool in_frame_tree_;

  RenderViewImpl* render_view_;
  int routing_id_;

  // If this RenderFrame was created to replace a previous object, this will
  // store its routing id. The previous object can be:
  // - A RenderFrame. This requires RenderDocument to be enabled.
  // - A RenderFrameProxy.
  // At commit time, the two objects will be swapped and the old one cleared.
  int previous_routing_id_;

  // Non-null when the RenderFrame is a local root for compositing, input,
  // layout, etc. A local frame is also a local root iff it does not have a
  // parent that is a local frame.
  RenderWidget* render_widget_ = nullptr;

  // If this is a main frame, the RenderView owns the RenderWidget and this
  // member is null. If this is a child frame, then this object owns the
  // RenderWidget and this member is not null.
  std::unique_ptr<RenderWidget> owned_render_widget_;

  // Keeps track of which future subframes the browser process has history items
  // for during a history navigation, as well as whether those items are for
  // about:blank.  The renderer process should ask the browser for history items
  // when subframes with these names are created (as long as they are not
  // staying at about:blank), and directly load the initial URLs for any other
  // subframes.
  //
  // This state is incrementally cleared as it is used and then reset in
  // didStopLoading, since it is not needed after the first load completes and
  // is never used after the initial navigation.
  // TODO(creis): Expand this to include any corresponding same-process
  // PageStates for the whole subtree in https://crbug.com/639842.
  base::flat_map<std::string, bool> history_subframe_unique_names_;

  // Stores the current history item for this frame, so that updates to it can
  // be reported to the browser process via SendUpdateState.
  blink::WebHistoryItem current_history_item_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Current text input composition text. Empty if no composition is in
  // progress.
  base::string16 pepper_composition_text_;

  PluginPowerSaverHelper* plugin_power_saver_helper_;
#endif

  // All the registered observers.
  base::ObserverList<RenderFrameObserver>::Unchecked observers_;

  // External context menu requests we're waiting for. "Internal"
  // (WebKit-originated) context menu events will have an ID of 0 and will not
  // be in this map.
  //
  // We don't want to add internal ones since some of the "special" page
  // handlers in the browser process just ignore the context menu requests so
  // avoid showing context menus, and so this will cause right clicks to leak
  // entries in this map. Most users of the custom context menu (e.g. Pepper
  // plugins) are normally only on "regular" pages and the regular pages will
  // always respond properly to the request, so we don't have to worry so
  // much about leaks.
  base::IDMap<ContextMenuClient*> pending_context_menus_;

  // The text selection the last time DidChangeSelection got called. May contain
  // additional characters before and after the selected text, for IMEs. The
  // portion of this string that is the actual selected text starts at index
  // |selection_range_.GetMin() - selection_text_offset_| and has length
  // |selection_range_.length()|.
  base::string16 selection_text_;
  // The offset corresponding to the start of |selection_text_| in the document.
  size_t selection_text_offset_;
  // Range over the document corresponding to the actual selected text (which
  // could correspond to a substring of |selection_text_|; see above).
  gfx::Range selection_range_;
  // Used to inform didChangeSelection() when it is called in the context
  // of handling a FrameInputHandler::SelectRange IPC.
  bool handling_select_range_;

  // Implements getUserMedia() and related functionality.
  std::unique_ptr<blink::WebMediaStreamDeviceObserver>
      web_media_stream_device_observer_;

  mojo::Remote<mojom::RendererAudioInputStreamFactory>
      audio_input_stream_factory_;

  // The media permission dispatcher attached to this frame.
  std::unique_ptr<MediaPermissionDispatcher> media_permission_dispatcher_;

  service_manager::BinderRegistry registry_;
  service_manager::InterfaceProvider remote_interfaces_;
  std::unique_ptr<BlinkInterfaceRegistryImpl> blink_interface_registry_;

  blink::BrowserInterfaceBrokerProxy browser_interface_broker_proxy_;

  // The current accessibility mode.
  ui::AXMode accessibility_mode_;

  // Only valid if |accessibility_mode_| has |ui::AXMode::kWebContents|
  // flag set.
  RenderAccessibilityImpl* render_accessibility_;

  // Whether or not this RenderFrame is currently pasting.
  bool is_pasting_;

  // Whether we must stop creating nested run loops for modal dialogs. This
  // is necessary because modal dialogs have a ScopedPageLoadDeferrer on the
  // stack that interferes with swapping out.
  bool suppress_further_dialogs_;

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // The external popup for the currently showing select popup.
  std::unique_ptr<ExternalPopupMenu> external_popup_menu_;
#endif

  std::unique_ptr<FrameBlameContext> blame_context_;

  // Plugins -------------------------------------------------------------------
#if BUILDFLAG(ENABLE_PLUGINS)
  typedef std::set<PepperPluginInstanceImpl*> PepperPluginSet;
  PepperPluginSet active_pepper_instances_;

  // Whether or not the focus is on a PPAPI plugin
  PepperPluginInstanceImpl* focused_pepper_plugin_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |pepper_last_mouse_event_target_| is not owned by this class. We depend on
  // the RenderFrameImpl to NULL it out when it destructs.
  PepperPluginInstanceImpl* pepper_last_mouse_event_target_;
#endif

  using AutoplayOriginAndFlags = std::pair<url::Origin, int32_t>;
  AutoplayOriginAndFlags autoplay_flags_;

  mojo::AssociatedReceiver<blink::mojom::AutoplayConfigurationClient>
      autoplay_configuration_receiver_{this};
  mojo::Receiver<mojom::Frame> frame_receiver_{this};
  mojo::AssociatedReceiver<mojom::FrameBindingsControl>
      frame_bindings_control_receiver_{this};
  mojo::AssociatedReceiver<mojom::FrameNavigationControl>
      frame_navigation_control_receiver_{this};
  mojo::AssociatedReceiver<mojom::FullscreenVideoElementHandler>
      fullscreen_receiver_{this};
  mojo::AssociatedReceiver<mojom::MhtmlFileWriter> mhtml_file_writer_receiver_{
      this};

  // Only used when PerNavigationMojoInterface is enabled.
  std::unique_ptr<NavigationClient> navigation_client_impl_;

  // Indicates whether |didAccessInitialDocument| was called.
  bool has_accessed_initial_document_;

  // Creates various media clients.
  MediaFactory media_factory_;

  blink::AssociatedInterfaceRegistry associated_interfaces_;
  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // This flag is true while browser process is processing a pending navigation,
  // as a result of mojom::FrameHost::BeginNavigation call. It is reset when the
  // navigation is either committed or cancelled.
  bool browser_side_navigation_pending_ = false;
  GURL browser_side_navigation_pending_url_;

  // A bitwise OR of bindings types that have been enabled for this RenderFrame.
  // See BindingsPolicy for details.
  int enabled_bindings_ = 0;

  // This boolean indicates whether JS bindings for Mojo should be enabled at
  // the time the next script context is created.
  bool enable_mojo_js_bindings_ = false;

  mojo::AssociatedRemote<mojom::FrameHost> frame_host_remote_;
  mojo::BindingSet<service_manager::mojom::InterfaceProvider>
      interface_provider_bindings_;

  // URLLoaderFactory instances used for subresource loading.
  // Depending on how the frame was created, |loader_factories_| could be:
  //   * |HostChildURLLoaderFactoryBundle| for standalone frames, or
  //   * |TrackedChildURLLoaderFactoryBundle| for frames opened by other frames.
  //
  // This must be updated only via SetLoaderFactoryBundle, which is called at a
  // certain timing - right before the new document is committed during
  // FrameLoader::CommitNavigation.
  scoped_refptr<ChildURLLoaderFactoryBundle> loader_factories_;

  scoped_refptr<FrameRequestBlocker> frame_request_blocker_;

  // AndroidOverlay routing token from the browser, if we have one yet.
  base::Optional<base::UnguessableToken> overlay_routing_token_;

  // Callbacks that we should call when we get a routing token.
  std::vector<media::RoutingTokenCallback> pending_routing_token_callbacks_;

  InputTargetClientImpl input_target_client_impl_;

  // Used for devtools instrumentation and trace-ability. This token is
  // used to tag calls and requests in order to attribute them to the context
  // frame.
  // |devtools_frame_token_| is only defined by the browser and is never
  // sent back from the renderer in the control calls.
  base::UnguessableToken devtools_frame_token_;

  // Bookkeeping to suppress redundant scroll and focus requests for an already
  // scrolled and focused editable node.
  bool has_scrolled_focused_editable_node_into_rect_ = false;
  gfx::Rect rect_for_scrolled_focused_editable_node_;

  // Contains a representation of the accessibility tree stored in content for
  // use inside of Blink.
  std::unique_ptr<blink::WebComputedAXTree> computed_ax_tree_;

  // Used for tracking the frame's size and replicating it to the browser
  // process when it changes.
  base::Optional<gfx::Size> frame_size_;

#if defined(OS_MACOSX)
  // Return the mojo interface for making ClipboardHost calls.
  mojo::Remote<blink::mojom::ClipboardHost> clipboard_host_;
#endif

  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  // Variable to control burst of download requests.
  int num_burst_download_requests_ = 0;
  base::TimeTicks burst_download_start_time_;

  RenderFrameMediaPlaybackOptions renderer_media_playback_options_;

  // Used by renderer initiated navigations to about:blank pages. This edge case
  // doesn't use yet the normal code path in the browser process.
  // TODO(arthursonzogni): Remove this. Everything should use the default code
  // path and be driven by the browser process.
  base::CancelableOnceCallback<void()> sync_navigation_callback_;

  class MHTMLBodyLoaderClient;
  std::unique_ptr<MHTMLBodyLoaderClient> mhtml_body_loader_client_;

  std::unique_ptr<blink::WebURLLoaderFactoryForTest>
      web_url_loader_factory_override_for_test_;

  base::WeakPtrFactory<RenderFrameImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
