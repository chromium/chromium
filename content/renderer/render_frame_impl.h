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
#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
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
#include "content/common/navigation_params.mojom.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/web_ui.mojom.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/fullscreen_video_element.mojom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/widget_type.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "content/renderer/content_security_policy_util.h"
#include "content/renderer/frame_blame_context.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/media/media_factory.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/routing_token_callback.h"
#include "media/base/speech_recognition_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
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
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info_notifier.mojom.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/renderer/pepper/plugin_power_saver_helper.h"
#endif

struct FrameMsg_MixedContentFound_Params;

namespace blink {
class WebComputedAXTree;
class WebContentDecryptionModule;
class WebElement;
class WebFrameRequestBlocker;
class WebLocalFrame;
class WebMediaStreamDeviceObserver;
class WebSecurityOrigin;
class WebString;
class WebURL;
struct FramePolicy;
struct WebContextMenuData;
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

class AgentSchedulingGroup;
class BlinkInterfaceRegistryImpl;
class CompositorDependencies;
class DocumentState;
class MediaPermissionDispatcher;
class NavigationClient;
class PepperPluginInstanceImpl;
class RendererPpapiHost;
class RenderAccessibilityManager;
class RenderFrameObserver;
class RenderViewImpl;
class RenderWidget;
class RenderWidgetFullscreenPepper;
struct CustomContextMenuContext;
struct FrameReplicationState;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      blink::mojom::AutoplayConfigurationClient,
      blink::mojom::ResourceLoadInfoNotifier,
      mojom::Frame,
      mojom::FrameNavigationControl,
      mojom::FullscreenVideoElementHandler,
      mojom::FrameBindingsControl,
      mojom::MhtmlFileWriter,
      public blink::WebLocalFrameClient,
      service_manager::mojom::InterfaceProvider {
 public:
  // Creates a new RenderFrame as the main frame of |render_view|.
  static RenderFrameImpl* CreateMainFrame(
      AgentSchedulingGroup& agent_scheduling_group,
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
      AgentSchedulingGroup& agent_scheduling_group,
      int routing_id,
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
          interface_provider,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      int previous_routing_id,
      const base::Optional<base::UnguessableToken>& opener_frame_token,
      int parent_routing_id,
      int previous_sibling_routing_id,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const FrameReplicationState& replicated_state,
      CompositorDependencies* compositor_deps,
      mojom::CreateFrameWidgetParamsPtr widget_params,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
      bool has_committed_real_load);

  // Returns the RenderFrameImpl for the given routing ID.
  static RenderFrameImpl* FromRoutingID(int routing_id);

  // Just like RenderFrame::FromWebFrame but returns the implementation.
  static RenderFrameImpl* FromWebFrame(blink::WebFrame* web_frame);

  // Constructor parameters are bundled into a struct.
  struct CONTENT_EXPORT CreateParams {
    CreateParams(AgentSchedulingGroup& agent_scheduling_group,
                 RenderViewImpl* render_view,
                 int32_t routing_id,
                 mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
                     interface_provider,
                 mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
                     browser_interface_broker,
                 const base::UnguessableToken& devtools_frame_token);
    ~CreateParams();

    CreateParams(CreateParams&&);
    CreateParams& operator=(CreateParams&&);

    AgentSchedulingGroup* agent_scheduling_group;
    RenderViewImpl* render_view;
    int32_t routing_id;
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider;
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker;
    base::UnguessableToken devtools_frame_token;
  };

  using CreateRenderFrameImplFunction = RenderFrameImpl* (*)(CreateParams);

  // Web tests override the creation of RenderFrames in order to inject a
  // partial testing fake.
  static void InstallCreateHook(CreateRenderFrameImplFunction create_frame);

  // Looks up and returns the WebFrame corresponding to a given frame routing
  // ID.
  static blink::WebFrame* ResolveWebFrame(int opener_frame_routing_id);

  // Possibly set the kOpenerCrossOrigin and kSandboxNoGesture policy in
  // |download_policy|.
  static void MaybeSetDownloadFramePolicy(
      bool is_opener_navigation,
      const blink::WebURLRequest& request,
      const blink::WebSecurityOrigin& current_origin,
      bool has_download_sandbox_flag,
      bool blocking_downloads_in_sandbox_enabled,
      bool from_ad,
      NavigationDownloadPolicy* download_policy);

  // Overwrites the given URL to use an HTML5 embed if possible.
  blink::WebURL OverrideFlashEmbedWithHTML(const blink::WebURL& url) override;

  ~RenderFrameImpl() override;

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
  // Returns the blink::WebFrameWidget attached to the RenderWidget that is
  // associated with this frame.
  blink::WebFrameWidget* GetLocalRootWebFrameWidget();

  // This method must be called after the WebLocalFrame backing this RenderFrame
  // has been created and added to the frame tree. It creates all objects that
  // depend on the frame being at its proper spot.
  //
  // Virtual for web tests to inject their own behaviour into the WebLocalFrame.
  virtual void Initialize(blink::WebFrame* parent);

  // Start/Stop loading notifications.
  // TODO(nasko): Those are page-level methods at this time and come from
  // WebViewClient. We should move them to be WebLocalFrameClient calls and put
  // logic in the browser side to balance starts/stops.
  void DidStartLoading() override;
  void DidStopLoading() override;

  // Returns the object implementing the RenderAccessibility mojo interface and
  // serves as a bridge between RenderFrameImpl and RenderAccessibilityImpl.
  RenderAccessibilityManager* GetRenderAccessibilityManager() {
    return render_accessibility_manager_.get();
  }

  // Called from RenderAccessibilityManager to let the RenderFrame know when the
  // accessibility mode has changed, so that it can notify its observers.
  void NotifyAccessibilityModeChange(ui::AXMode new_mode);

  // Whether or not the frame is currently swapped into the frame tree.  If
  // this is false, this is a provisional frame which has not committed yet,
  // and which will swap with a proxy when it commits.
  //
  // TODO(https://crbug.com/578349): Remove this once provisional frames are
  // gone, and clean up code that depends on it.
  bool in_frame_tree() { return in_frame_tree_; }

  // A RenderView opened by this RenderFrame needs to be shown.
  void ShowCreatedWindow(bool opened_by_user_gesture,
                         RenderWidget* render_widget_to_show,
                         blink::WebNavigationPolicy policy,
                         const gfx::Rect& initial_rect);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Notification that a PPAPI plugin has been created.
  void PepperPluginCreated(RendererPpapiHost* host);

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
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      int selection_start,
      int selection_end);
  void SimulateImeCommitText(const base::string16& text,
                             const std::vector<ui::ImeTextSpan>& ime_text_spans,
                             const gfx::Range& replacement_range);

  // TODO(jam): remove these once the IPC handler moves from RenderView to
  // RenderFrame.
  void OnImeSetComposition(const base::string16& text,
                           const std::vector<ui::ImeTextSpan>& ime_text_spans,
                           int selection_start,
                           int selection_end);
  void OnImeCommitText(const base::string16& text,
                       const gfx::Range& replacement_range,
                       int relative_cursor_pos);
  void OnImeFinishComposingText(bool keep_selection);

#endif  // BUILDFLAG(ENABLE_PLUGINS)

  void ScriptedPrint(bool user_initiated);

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
  std::unique_ptr<AXTreeSnapshotter> CreateAXTreeSnapshotter() override;
  int GetRoutingID() override;
  blink::WebLocalFrame* GetWebFrame() override;
  const blink::web_pref::WebPreferences& GetBlinkPreferences() override;
  int ShowContextMenu(ContextMenuClient* client,
                      const UntrustworthyContextMenuParams& params) override;
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
  void AllowlistContentOrigin(const url::Origin& content_origin) override;
  void PluginDidStartLoading() override;
  void PluginDidStopLoading() override;
#endif
  bool IsFTPDirectoryListing() override;
  void SetSelectedText(const base::string16& selection_text,
                       size_t offset,
                       const gfx::Range& range) override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  blink::PreviewsState GetPreviewsState() override;
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
  void SetAllowsCrossBrowsingInstanceFrameLookup() override;
  gfx::RectF ElementBoundsInWindow(const blink::WebElement& element) override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  float GetDeviceScaleFactor() override;

  // blink::mojom::AutoplayConfigurationClient implementation:
  void AddAutoplayFlags(const url::Origin& origin,
                        const int32_t flags) override;

  // These are called for dedicated workers only when
  // PlzDedicatedWorker is enabled.
  // blink::mojom::ResourceLoadInfoNotifier implementation:
  void NotifyResourceRedirectReceived(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr redirect_response) override;
  void NotifyResourceResponseReceived(
      blink::mojom::ResourceLoadInfoPtr resource_load_info,
      network::mojom::URLResponseHeadPtr head,
      int32_t previews_state) override;
  void NotifyResourceTransferSizeUpdated(int32_t request_id,
                                         int32_t transfer_size_diff) override;
  void NotifyResourceLoadCompleted(
      blink::mojom::ResourceLoadInfoPtr resource_load_info,
      const ::network::URLLoaderCompletionStatus& status) override;
  void NotifyResourceLoadCanceled(int32_t request_id) override;
  void Clone(mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier>
                 pending_resource_load_info_notifier) override;

  // mojom::Frame implementation:
  void GetInterfaceProvider(
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver)
      override;
  void GetCanonicalUrlForSharing(
      GetCanonicalUrlForSharingCallback callback) override;
  void BlockRequests() override;
  void ResumeBlockedRequests() override;
  void CancelBlockedRequests() override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;
  void SnapshotAccessibilityTree(
      uint32_t ax_mode,
      SnapshotAccessibilityTreeCallback callback) override;
  void GetSerializedHtmlWithLocalLinks(
      const base::flat_map<GURL, base::FilePath>& url_map,
      const base::flat_map<base::UnguessableToken, base::FilePath>&
          frame_token_map,
      bool save_with_empty_url,
      mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> handler_remote)
      override;

#if defined(OS_ANDROID)
  void ExtractSmartClipData(
      const gfx::Rect& rect,
      const ExtractSmartClipDataCallback callback) override;
#endif

  // mojom::FrameBindingsControl implementation:
  void AllowBindings(int32_t enabled_bindings_flags) override;
  void EnableMojoJsBindings() override;
  void BindWebUI(mojo::PendingReceiver<mojom::WebUI> Receiver,
                 mojo::PendingRemote<mojom::WebUIHost> remote) override;

  // These mirror mojom::NavigationClient, called by NavigationClient.
  void CommitNavigation(
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
      const base::UnguessableToken& devtools_navigation_token,
      mojom::NavigationClient::CommitNavigationCallback commit_callback);
  void CommitFailedNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      net::ResolveErrorInfo resolve_error_info,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      mojom::NavigationClient::CommitFailedNavigationCallback
          per_navigation_mojo_interface_callback);

  // mojom::FrameNavigationControl implementation:
  void CommitSameDocumentNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      CommitSameDocumentNavigationCallback callback) override;
  void HandleRendererDebugURL(const GURL& url) override;
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories) override;
  void BindDevToolsAgent(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver)
      override;
  void JavaScriptMethodExecuteRequest(
      const base::string16& object_name,
      const base::string16& method_name,
      base::Value arguments,
      bool wants_result,
      JavaScriptMethodExecuteRequestCallback callback) override;
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
  void SwapIn() override;

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
#if !defined(OS_ANDROID)
  std::unique_ptr<media::SpeechRecognitionClient> CreateSpeechRecognitionClient(
      media::SpeechRecognitionClient::OnReadyCallback callback) override;
#endif
  scoped_refptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext()
      override;
  scoped_refptr<blink::WebWorkerFetchContext>
  CreateWorkerFetchContextForPlzDedicatedWorker(
      blink::WebDedicatedWorkerHostFactoryClient* factory_client) override;
  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking()
      override;
  blink::BlameContext* GetFrameBlameContext() override;
  std::unique_ptr<blink::WebServiceWorkerProvider> CreateServiceWorkerProvider()
      override;
  blink::AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces()
      override;
  blink::WebLocalFrame* CreateChildFrame(
      blink::WebLocalFrame* parent,
      blink::mojom::TreeScopeType scope,
      const blink::WebString& name,
      const blink::WebString& fallback_name,
      const blink::FramePolicy& frame_policy,
      const blink::WebFrameOwnerProperties& frame_owner_properties,
      blink::mojom::FrameOwnerElementType frame_owner_element_type) override;
  std::pair<blink::WebRemoteFrame*, blink::PortalToken> CreatePortal(
      blink::CrossVariantMojoAssociatedReceiver<
          blink::mojom::PortalInterfaceBase> portal_endpoint,
      blink::CrossVariantMojoAssociatedRemote<
          blink::mojom::PortalClientInterfaceBase> client_endpoint,
      const blink::WebElement& portal_element) override;
  blink::WebRemoteFrame* AdoptPortal(
      const blink::PortalToken& portal_token,
      const blink::WebElement& portal_element) override;
  blink::WebFrame* FindFrame(const blink::WebString& name) override;
  void WillDetach() override;
  void FrameDetached() override;
  void DidChangeName(const blink::WebString& name) override;
  void DidSetFramePolicyHeaders(
      network::mojom::WebSandboxFlags flags,
      const blink::ParsedFeaturePolicy& fp_header,
      const blink::DocumentPolicyFeatureState& dp_header) override;
  void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
      override;
  bool ShouldReportDetailedMessageForSource(
      const blink::WebString& source) override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void DidCreateDocumentLoader(
      blink::WebDocumentLoader* document_loader) override;
  void DidCommitNavigation(const blink::WebHistoryItem& item,
                           blink::WebHistoryCommitType commit_type,
                           bool should_reset_browser_interface_broker) override;
  void DidCreateInitialEmptyDocument() override;
  void DidCommitDocumentReplacementNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidClearWindowObject() override;
  void DidCreateDocumentElement() override;
  void RunScriptsAtDocumentElementAvailable() override;
  void DidReceiveTitle(const blink::WebString& title) override;
  void DidFinishDocumentLoad() override;
  void RunScriptsAtDocumentReady(bool document_is_empty) override;
  void RunScriptsAtDocumentIdle() override;
  void DidHandleOnloadEvents() override;
  void DidFinishLoad() override;
  void DidFinishSameDocumentNavigation(const blink::WebHistoryItem& item,
                                       blink::WebHistoryCommitType commit_type,
                                       bool content_initiated) override;
  void DidUpdateCurrentHistoryItem() override;
  base::UnguessableToken GetDevToolsFrameToken() override;
  void AbortClientNavigation() override;
  void DidChangeSelection(bool is_empty_selection) override;
  void ShowContextMenu(
      const blink::WebContextMenuData& data,
      const base::Optional<gfx::Point>& host_context_menu_location) override;
  void FrameRectsChanged(const blink::WebRect& frame_rect) override;
  void FocusedElementChanged(const blink::WebElement& element) override;
  void OnMainFrameIntersectionChanged(
      const blink::WebRect& intersect_rect) override;
  void WillSendRequest(blink::WebURLRequest& request,
                       ForRedirect for_redirect) override;
  void DidLoadResourceFromMemoryCache(
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response) override;
  void DidChangePerformanceTiming() override;
  void DidObserveInputDelay(base::TimeDelta input_delay) override;
  void DidChangeCpuTiming(base::TimeDelta time) override;
  void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior) override;
  void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature) override;
  void DidObserveNewCssPropertyUsage(blink::mojom::CSSSampleId css_property,
                                     bool is_animated) override;
  void DidObserveLayoutShift(double score, bool after_input_or_scroll) override;
  void DidObserveLayoutNg(uint32_t all_block_count,
                          uint32_t ng_block_count,
                          uint32_t all_call_count,
                          uint32_t ng_call_count) override;
  void DidObserveLazyLoadBehavior(
      blink::WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) override;
  bool ShouldTrackUseCounter(const blink::WebURL& url) override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override;
  void DidChangeScrollOffset() override;
  blink::WebMediaStreamDeviceObserver* MediaStreamDeviceObserver() override;
  bool AllowRTCLegacyTLSProtocols() override;
  blink::WebEncryptedMediaClient* EncryptedMediaClient() override;
  blink::WebString UserAgentOverride() override;
  base::Optional<blink::UserAgentMetadata> UserAgentMetadataOverride() override;
  blink::WebString DoNotTrackValue() override;
  blink::mojom::RendererAudioInputStreamFactory* GetAudioInputStreamFactory();
  bool AllowContentInitiatedDataUrlNavigations(
      const blink::WebURL& url) override;
  void PostAccessibilityEvent(const ui::AXEvent& event) override;
  void MarkWebAXObjectDirty(const blink::WebAXObject& obj,
                            bool subtree) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      blink::WebSetSinkIdCompleteCallback callback) override;
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory() override;
  void OnStopLoading() override;
  void MaybeProxyURLLoaderFactory(
      blink::CrossVariantMojoReceiver<
          network::mojom::URLLoaderFactoryInterfaceBase>* factory_receiver)
      override;
  void DraggableRegionsChanged() override;
  blink::BrowserInterfaceBrokerProxy* GetBrowserInterfaceBroker() override;
  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed.
  // TODO(varunjain): delete this method once we figure out how to keep
  // selection handles in sync with the webpage.
  void SyncSelectionIfRequired() override;
  void ScrollFocusedEditableElementIntoRect(const gfx::Rect& rect) override;
  void ResetHasScrolledFocusedEditableIntoView() override;
  void CreateAudioInputStream(
      blink::CrossVariantMojoRemote<
          blink::mojom::RendererAudioInputStreamFactoryClientInterfaceBase>
          client,
      const base::UnguessableToken& session_id,
      const media::AudioParameters& params,
      bool automatic_gain_control,
      uint32_t shared_memory_count) override;
  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void DidCommitAndDrawCompositorFrame() override;
  void WasHidden() override;
  void WasShown() override;

  void SetUpSharedMemoryForSmoothness(
      base::ReadOnlySharedMemoryRegion shared_memory);

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

  // Sends the current frame's navigation state to the browser.
  void SendUpdateState();

  // Creates a MojoBindingsController if Mojo bindings have been enabled for
  // this frame. For WebUI, this allows the page to communicate with the browser
  // process; for layout tests, this allows the test to mock out services at
  // the Mojo IPC layer.
  void MaybeEnableMojoBindings();

  void NotifyObserversOfFailedProvisionalLoad();

  // Plugin-related functions --------------------------------------------------

#if BUILDFLAG(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin() {
    return focused_pepper_plugin_;
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

  // Called when an ongoing renderer-initiated navigation was dropped by the
  // browser.
  void OnDroppedNavigation();

  void DidStartResponse(const GURL& response_url,
                        int request_id,
                        network::mojom::URLResponseHeadPtr response_head,
                        network::mojom::RequestDestination request_destination,
                        blink::PreviewsState previews_state);
  void DidCompleteResponse(int request_id,
                           const network::URLLoaderCompletionStatus& status);
  void DidCancelResponse(int request_id);
  void DidReceiveTransferSizeUpdate(int request_id, int received_data_length);

  bool GetCaretBoundsFromFocusedPlugin(gfx::Rect& rect) override;

  // Used in tests to install a fake WebURLLoaderFactory via
  // RenderViewTest::CreateFakeWebURLLoaderFactory().
  void SetWebURLLoaderFactoryOverrideForTest(
      std::unique_ptr<blink::WebURLLoaderFactoryForTest> factory);

 protected:
  explicit RenderFrameImpl(CreateParams params);

  bool IsLocalRoot() const;
  const RenderFrameImpl* GetLocalRoot() const;

  // Gets the unique_name() of the frame being replaced by this frame, when
  // it is a provisional frame. Invalid to call on frames that are already
  // attached to the frame tree.
  std::string GetPreviousFrameUniqueName();

 private:
  friend class RenderFrameImplTest;
  friend class RenderFrameObserver;
  friend class TestRenderFrame;

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
      AgentSchedulingGroup& agent_scheduling_group,
      RenderViewImpl* render_view,
      int32_t routing_id,
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
          interface_provider,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      const base::UnguessableToken& devtools_frame_token);

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  // Swaps the current frame into the frame tree, replacing the
  // RenderFrameProxy it is associated with. Return value indicates whether
  // the swap operation succeeded. This should only be used for provisional
  // frames associated with a proxy, while the proxy is still in the frame tree.
  // If the associated proxy has been detached before this is called, this
  // returns false and aborts the swap.
  bool SwapInInternal();

  // Returns the RenderWidget associated with the main frame.
  // TODO(ajwong): This method should go away when cross-frame property setting
  // events moves into RenderWidget.
  RenderWidget* GetMainFrameRenderWidget();

  // Checks whether accessibility support for this frame is currently enabled.
  bool IsAccessibilityEnabled() const;

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnUnload(int proxy_routing_id,
                bool is_loading,
                const FrameReplicationState& replicated_frame_state,
                const base::UnguessableToken& frame_token);
  void OnDeleteFrame(FrameDeleteIntention intent);
  void OnShowContextMenu(const gfx::Point& location);
  void OnContextMenuClosed(const CustomContextMenuContext& custom_context);
  void OnCustomContextMenuAction(const CustomContextMenuContext& custom_context,
                                 unsigned action);
  void OnMoveCaret(const gfx::Point& point);
  void OnScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);
  void OnSelectRange(const gfx::Point& base, const gfx::Point& extent);
  void OnSuppressFurtherDialogs();
  void OnMixedContentFound(const FrameMsg_MixedContentFound_Params& params);

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

  // Returns a mostly empty bundle, with a fallback that uses a process-wide,
  // direct-network factory.
  //
  // TODO(lukasza): https://crbug.com/1114822: Remove once the fallback is no
  // longer needed.
  scoped_refptr<ChildURLLoaderFactoryBundle> GetLoaderFactoryBundleFallback();

  scoped_refptr<ChildURLLoaderFactoryBundle> CreateLoaderFactoryBundle(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> info,
      base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory);

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

  base::Value GetJavaScriptExecutionResult(v8::Local<v8::Value> result);

  void InitializeMediaStreamDeviceObserver();

  // Does preparation for the navigation to |url|.
  void PrepareRenderViewForNavigation(
      const GURL& url,
      const mojom::CommitNavigationParams& commit_params);

  // Sends a FrameHostMsg_BeginNavigation to the browser
  void BeginNavigationInternal(std::unique_ptr<blink::WebNavigationInfo> info,
                               bool is_history_navigation_in_new_child_frame,
                               base::TimeTicks renderer_before_unload_start,
                               base::TimeTicks renderer_before_unload_end);

  // Used to load the initial empty document. This one is special, since it
  // isn't the result of a navigation.
  // TODO(rakina): Rename the method now that this is only used for the
  // special case of committing the initial empty document.
  void CommitSyncNavigation(std::unique_ptr<blink::WebNavigationInfo> info);

  // Commit navigation with |navigation_params| prepared.
  void CommitNavigationWithParams(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
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

  // |transition_type| corresponds to the document which triggered this request.
  void WillSendRequestInternal(blink::WebURLRequest& request,
                               bool for_main_frame,
                               ui::PageTransition transition_type,
                               ForRedirect for_redirect);

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

  // Send |callback| our AndroidOverlay routing token.
  void RequestOverlayRoutingToken(media::RoutingTokenCallback callback);

  void BindWebUIReceiver(mojo::PendingReceiver<mojom::WebUI> receiver);

  void ShowDeferredContextMenu(const UntrustworthyContextMenuParams& params);

  // Build DidCommitProvisionalLoad_Params based on the frame internal state.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  MakeDidCommitProvisionalLoadParams(
      blink::WebHistoryCommitType commit_type,
      ui::PageTransition transition,
      const base::Optional<base::UnguessableToken>& embedding_token);

  // Updates the navigation history depending on the passed parameters.
  // This could result either in the creation of a new entry or a modification
  // of the current entry or nothing. If a new entry was created,
  // returns true, false otherwise.
  void UpdateNavigationHistory(const blink::WebHistoryItem& item,
                               blink::WebHistoryCommitType commit_type);

  // Notify render_view_ observers that a commit happened.
  void NotifyObserversOfNavigationCommit(ui::PageTransition transition);

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
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
      const base::Optional<base::UnguessableToken>& embedding_token);

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
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> info) override;

  // Updates the state of this frame when asked to commit a navigation.
  void PrepareFrameForCommit(
      const GURL& url,
      const mojom::CommitNavigationParams& commit_params);

  // Returns true if UA (and UA client hints) overrides in renderer preferences
  // should be used.
  bool ShouldUseUserAgentOverride() const;

  // Sets the PageLifecycleState and runs pagehide and visibilitychange handlers
  // of the old page before committing this RenderFrame. Should only be called
  // for main-frame same-site navigations where we did a proactive
  // BrowsingInstance swap and we're reusing the old page's process. This is
  // needed to ensure consistency with other same-site main frame navigations.
  // Note that we will set the page's visibility to hidden, but not run the
  // unload handlers of the old page, nor actually unload/freeze the page here.
  // That needs a more complicated support on the browser side which will be
  // implemented later.
  // TODO(crbug.com/1110744): Support unload-in-commit.
  void SetOldPageLifecycleStateFromNewPageCommitIfNeeded(
      const mojom::OldPageInfo* old_page_info);

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

  // Ignores the navigation commit and stop its processing in the RenderFrame.
  // This will drop the NavigationRequest in the RenderFrameHost.
  // Note: This is only meant to be used before building the DocumentState.
  // Commit abort and navigation end is handled by it afterwards.
  void AbortCommitNavigation();

  // Implements AddMessageToConsole().
  void AddMessageToConsoleImpl(blink::mojom::ConsoleMessageLevel level,
                               const std::string& message,
                               bool discard_duplicates);

  // Stores the WebLocalFrame we are associated with.  This is null from the
  // constructor until BindToFrame() is called, and it is null after
  // FrameDetached() is called until destruction (which is asynchronous in the
  // case of the main frame, but not subframes).
  blink::WebNavigationControl* frame_ = nullptr;

  // The `AgentSchedulingGroup` this frame is associated with.
  AgentSchedulingGroup& agent_scheduling_group_;

  // Boolean value indicating whether this RenderFrameImpl object is for the
  // main frame or not. It remains accurate during destruction, even when
  // |frame_| has been invalidated.
  bool is_main_frame_;

  class UniqueNameFrameAdapter : public blink::UniqueNameHelper::FrameAdapter {
   public:
    explicit UniqueNameFrameAdapter(RenderFrameImpl* render_frame);
    ~UniqueNameFrameAdapter() override;

    // FrameAdapter overrides:
    bool IsMainFrame() const override;
    bool IsCandidateUnique(base::StringPiece name) const override;
    int GetSiblingCount() const override;
    int GetChildCount() const override;
    std::vector<std::string> CollectAncestorNames(
        BeginPoint begin_point,
        bool (*should_stop)(base::StringPiece)) const override;
    std::vector<int> GetFramePosition(BeginPoint begin_point) const override;

   private:
    blink::WebLocalFrame* GetWebFrame() const;

    RenderFrameImpl* render_frame_;
  };
  UniqueNameFrameAdapter unique_name_frame_adapter_;
  blink::UniqueNameHelper unique_name_helper_;

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
  const int routing_id_;

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

  // Implements getUserMedia() and related functionality.
  std::unique_ptr<blink::WebMediaStreamDeviceObserver>
      web_media_stream_device_observer_;

  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory>
      audio_input_stream_factory_;

  // The media permission dispatcher attached to this frame.
  std::unique_ptr<MediaPermissionDispatcher> media_permission_dispatcher_;

  service_manager::BinderRegistry registry_;
  service_manager::InterfaceProvider remote_interfaces_;
  std::unique_ptr<BlinkInterfaceRegistryImpl> blink_interface_registry_;

  blink::BrowserInterfaceBrokerProxy browser_interface_broker_proxy_;

  // Valid during the entire life time of the RenderFrame.
  std::unique_ptr<RenderAccessibilityManager> render_accessibility_manager_;

  std::unique_ptr<FrameBlameContext> blame_context_;

  // Plugins -------------------------------------------------------------------
#if BUILDFLAG(ENABLE_PLUGINS)
  typedef std::set<PepperPluginInstanceImpl*> PepperPluginSet;
  PepperPluginSet active_pepper_instances_;

  // Whether or not the focus is on a PPAPI plugin
  PepperPluginInstanceImpl* focused_pepper_plugin_;
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
  mojo::ReceiverSet<service_manager::mojom::InterfaceProvider>
      interface_provider_receivers_;

  mojo::ReceiverSet<blink::mojom::ResourceLoadInfoNotifier>
      resource_load_info_notifier_receivers_;

  // URLLoaderFactory instances used for subresource loading.
  // Depending on how the frame was created, |loader_factories_| could be:
  //   * |HostChildURLLoaderFactoryBundle| for standalone frames, or
  //   * |TrackedChildURLLoaderFactoryBundle| for frames opened by other frames.
  //
  // This must be updated only via SetLoaderFactoryBundle, which is called at a
  // certain timing - right before the new document is committed during
  // FrameLoader::CommitNavigation.
  scoped_refptr<ChildURLLoaderFactoryBundle> loader_factories_;

  // Loader factory bundle is stored here temporary between CommitNavigation
  // and DidCommitNavigation calls. These happen synchronously one after
  // another.
  scoped_refptr<ChildURLLoaderFactoryBundle> pending_loader_factories_;

  scoped_refptr<blink::WebFrameRequestBlocker> frame_request_blocker_;

  // AndroidOverlay routing token from the browser, if we have one yet.
  base::Optional<base::UnguessableToken> overlay_routing_token_;

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

  // Used for tracking a frame's main frame document intersection and
  // and replicating it to the browser when it changes.
  base::Optional<blink::WebRect> mainframe_intersection_rect_;

  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  RenderFrameMediaPlaybackOptions renderer_media_playback_options_;

  class MHTMLBodyLoaderClient;
  std::unique_ptr<MHTMLBodyLoaderClient> mhtml_body_loader_client_;

  std::unique_ptr<blink::WebURLLoaderFactoryForTest>
      web_url_loader_factory_override_for_test_;

  // When the browser asks the renderer to commit a navigation, it should always
  // result in a committed navigation reported via DidCommitProvisionalLoad().
  // This is important because DidCommitProvisionalLoad() is responsible for
  // swapping in the provisional local frame during a cross-process navigation.
  // Since this involves updating state in both the browser process and the
  // renderer process, this assert ensures that the state remains synchronized
  // between the two processes.
  //
  // Note: there is one exception that can result in no commit happening.
  // Committing a navigation runs unload handlers, which can detach |this|. In
  // that case, it doesn't matter that the navigation never commits, since the
  // logical node for |this| has been removed from the DOM.
  enum class NavigationCommitState {
    // Represents the initial empty document. This is represented separately
    // from |kNone| because Blink does not report the commit of the initial
    // empty document in a newly created frame. However, note that there are
    // some surprising quirks:
    //
    //   <iframe></iframe>
    //
    // will *not* be in the |kInitialEmptyDocument| state: while it initially
    // starts at the initial empty document, the initial empty document is then
    // synchronously replaced with a navigation to about:blank. In contrast:
    //
    //   <iframe src="https://slow.example.com"></iframe>
    //
    // will be in |kInitialEmptyDocument| until the navigation to
    // https://slow.example.com commits.
    kInitialEmptyDocument,
    // No commit in progress. This state also implies that the frame is not
    // displaying the initial empty document.
    kNone,
    // Marks that an active commit attempt is on the stack.
    kWillCommit,
    // Marks an active commit attempt as successful.
    kDidCommit,
  };

  enum MayReplaceInitialEmptyDocumentTag {
    kMayReplaceInitialEmptyDocument,
  };

  class CONTENT_EXPORT AssertNavigationCommits {
   public:
    // Construct a new scoper to verify that a navigation commit attempt
    // succeeds. Asserts that:
    // - no navigation is in progress
    // - the frame is not displaying the initial empty document.
    explicit AssertNavigationCommits(RenderFrameImpl* frame);

    // Similar to the previous constructor but allows transitions from the
    // initial empty document.
    explicit AssertNavigationCommits(RenderFrameImpl* frame,
                                     MayReplaceInitialEmptyDocumentTag);

    ~AssertNavigationCommits();

   private:
    explicit AssertNavigationCommits(
        RenderFrameImpl* frame,
        bool allow_transition_from_initial_empty_document);

    const base::WeakPtr<RenderFrameImpl> frame_;
  };

  NavigationCommitState navigation_commit_state_ =
      NavigationCommitState::kInitialEmptyDocument;

  base::WeakPtrFactory<RenderFrameImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
