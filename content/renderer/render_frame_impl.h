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
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/download/mhtml_save_status.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_message_enums.h"
#include "content/common/host_zoom.mojom.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unique_name_helper.h"
#include "content/common/widget.mojom.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/fullscreen_video_element.mojom.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/widget_type.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "content/renderer/content_security_policy_util.h"
#include "content/renderer/frame_blame_context.h"
#include "content/renderer/input/input_target_client_impl.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/media/media_factory.h"
#include "content/renderer/renderer_webcookiejar_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/routing_token_callback.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/platform/autoplay.mojom.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/web/commit_result.mojom.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_icon_url.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"
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
struct FrameMsg_PostMessage_Params;
struct FrameMsg_SerializeAsMHTML_Params;
struct FrameMsg_TextTrackSettings_Params;

namespace blink {
class WebComputedAXTree;
class WebContentDecryptionModule;
class WebElement;
class WebLayerTreeView;
class WebLocalFrame;
class WebPushClient;
class WebRelatedAppsFetcher;
class WebSecurityOrigin;
class WebString;
class WebURL;
struct FramePolicy;
struct WebContextMenuData;
struct WebCursorInfo;
struct WebNavigationParams;
struct WebMediaPlayerAction;
struct WebImeTextSpan;
struct WebScrollIntoViewParams;

namespace mojom {
class FileChooserParams;
}
}  // namespace blink

namespace gfx {
class Point;
class Range;
}

namespace media {
class MediaPermission;
}

namespace network {
struct ResourceResponseHead;
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
class ExternalPopupMenu;
class FrameRequestBlocker;
class HistoryEntry;
class ManifestManager;
class MediaPermissionDispatcher;
class MediaStreamDeviceObserver;
class NavigationClient;
class PepperPluginInstanceImpl;
class PushMessagingClient;
class RelatedAppsFetcher;
class RenderAccessibilityImpl;
class RendererPpapiHost;
class RenderFrameObserver;
class RenderViewImpl;
class RenderWidget;
class RenderWidgetFullscreenPepper;
class SharedWorkerRepository;
class UserMediaClientImpl;
struct CSPViolationParams;
struct CommonNavigationParams;
struct CustomContextMenuContext;
struct FrameOwnerProperties;
struct FrameReplicationState;
struct RequestNavigationParams;
struct ScreenInfo;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      blink::mojom::AutoplayConfigurationClient,
      mojom::Frame,
      mojom::FrameNavigationControl,
      mojom::FullscreenVideoElementHandler,
      mojom::HostZoom,
      mojom::FrameBindingsControl,
      public blink::WebLocalFrameClient,
      public blink::WebFrameSerializerClient,
      service_manager::mojom::InterfaceProvider {
 public:
  // Creates a new RenderFrame as the main frame of |render_view|.
  static RenderFrameImpl* CreateMainFrame(
      RenderViewImpl* render_view,
      int32_t routing_id,
      service_manager::mojom::InterfaceProviderPtr interface_provider,
      int32_t widget_routing_id,
      bool hidden,
      const ScreenInfo& screen_info,
      CompositorDependencies* compositor_deps,
      blink::WebFrame* opener,
      const base::UnguessableToken& devtools_frame_token,
      const FrameReplicationState& replicated_state,
      bool has_committed_real_load);

  // Creates a new RenderFrame with |routing_id|.  If |proxy_routing_id| is
  // MSG_ROUTING_NONE, it creates the Blink WebLocalFrame and inserts it into
  // the frame tree after the frame identified by
  // |previous_sibling_routing_id|, or as the first child if
  // |previous_sibling_routing_id| is MSG_ROUTING_NONE. Otherwise, the frame is
  // semi-orphaned until it commits, at which point it replaces the proxy
  // identified by |proxy_routing_id|.
  // The frame's opener is set to the frame identified by |opener_routing_id|.
  // The frame is created as a child of the RenderFrame identified by
  // |parent_routing_id| or as the top-level frame if
  // the latter is MSG_ROUTING_NONE.
  // |devtools_frame_token| is passed from the browser and corresponds to the
  // owner FrameTreeNode.  It can only be used for tagging requests and calls
  // for context frame attribution. It should never be passed back to the
  // browser as a frame identifier in the control flows calls.
  //
  // Note: This is called only when RenderFrame is being created in response
  // to IPC message from the browser process. All other frame creation is driven
  // through Blink and Create.
  static void CreateFrame(
      int routing_id,
      service_manager::mojom::InterfaceProviderPtr interface_provider,
      int proxy_routing_id,
      int opener_routing_id,
      int parent_routing_id,
      int previous_sibling_routing_id,
      const base::UnguessableToken& devtools_frame_token,
      const FrameReplicationState& replicated_state,
      CompositorDependencies* compositor_deps,
      const mojom::CreateFrameWidgetParams& params,
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
        const base::UnguessableToken& devtools_frame_token);
    ~CreateParams();

    CreateParams(CreateParams&&);
    CreateParams& operator=(CreateParams&&);

    RenderViewImpl* render_view;
    int32_t routing_id;
    service_manager::mojom::InterfaceProviderPtr interface_provider;
    base::UnguessableToken devtools_frame_token;
  };

  using CreateRenderFrameImplFunction = RenderFrameImpl* (*)(CreateParams);
  using CreateRenderWidgetForChildLocalRootFunction =
      RenderWidget* (*)(int32_t,
                        CompositorDependencies*,
                        WidgetType,
                        const ScreenInfo&,
                        blink::WebDisplayMode display_mode,
                        bool,
                        bool,
                        bool);
  using RenderWidgetForChildLocalRootInitializedCallback =
      void (*)(RenderWidget*);

  // LayoutTests override the creation of RenderFrames and RenderWidgets in
  // order to inject their own (subclass) type and change behaviour inside the
  // tests.
  static void InstallCreateHook(
      CreateRenderFrameImplFunction create_frame,
      CreateRenderWidgetForChildLocalRootFunction create_widget,
      RenderWidgetForChildLocalRootInitializedCallback widget_initialized);

  // Looks up and returns the WebFrame corresponding to a given opener frame
  // routing ID.
  static blink::WebFrame* ResolveOpener(int opener_frame_routing_id);

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

  RendererWebCookieJarImpl* cookie_jar() { return &cookie_jar_; }

  // Returns the RenderWidget associated with this frame.
  RenderWidget* GetRenderWidget();

  // This method must be called after the frame has been added to the frame
  // tree. It creates all objects that depend on the frame being at its proper
  // spot.
  void Initialize();

  // Notifications from RenderWidget.
  void WasHidden();
  void WasShown();
  void WidgetWillClose();

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

  // The focused node changed to |node|. If focus was lost from this frame,
  // |node| will be null.
  void FocusedNodeChanged(const blink::WebNode& node);

  // TODO(dmazzoni): the only reason this is here is to plumb it through to
  // RenderAccessibilityImpl. It should use the RenderFrameObserver method, once
  // blink has a separate accessibility tree per frame.
  void FocusedNodeChangedForAccessibility(const blink::WebNode& node);

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
  void SimulateImeFinishComposingText(bool keep_selection);

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

  // May return NULL in some cases, especially if userMediaClient() returns
  // NULL.
  MediaStreamDeviceObserver* GetMediaStreamDeviceObserver();

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
  void BindToFrame(blink::WebLocalFrame* frame) override;
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
  void RegisterPeripheralPlugin(
      const url::Origin& content_origin,
      const base::Closure& unthrottle_callback) override;
  RenderFrame::PeripheralContentStatus GetPeripheralContentStatus(
      const url::Origin& main_frame_origin,
      const url::Origin& content_origin,
      const gfx::Size& unobscured_size,
      RecordPeripheralDecision record_decision) const override;
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
  void SetZoomLevel(double zoom_level) override;
  double GetZoomLevel() const override;
  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message) override;
  void SetPreviewsState(PreviewsState previews_state) override;
  PreviewsState GetPreviewsState() const override;
  bool IsPasting() const override;
  blink::mojom::PageVisibilityState GetVisibilityState() const override;
  bool IsBrowserSideNavigationPending() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      blink::TaskType task_type) override;
  int GetEnabledBindings() const override;
  void SetAccessibilityModeForTest(ui::AXMode new_mode) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

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
#if defined(OS_ANDROID)
  void ExtractSmartClipData(
      const gfx::Rect& rect,
      const ExtractSmartClipDataCallback callback) override;
#endif

  // mojom::FrameBindingsControl implementation:
  void AllowBindings(int32_t enabled_bindings_flags) override;

  // mojom::FrameNavigationControl implementation:
  void CommitNavigation(
      const network::ResourceResponseHead& head,
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
      network::mojom::URLLoaderFactoryPtr prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      CommitNavigationCallback callback) override;
  void CommitFailedNavigation(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      bool has_stale_copy_in_cache,
      int error_code,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders,
      CommitFailedNavigationCallback callback) override;
  void CommitSameDocumentNavigation(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      CommitSameDocumentNavigationCallback callback) override;
  void HandleRendererDebugURL(const GURL& url) override;
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders) override;
  void BindDevToolsAgent(
      blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
      blink::mojom::DevToolsAgentAssociatedRequest request) override;

  // mojom::FullscreenVideoElementHandler implementation:
  void RequestFullscreenVideoElement() override;

  // mojom::HostZoom implementation:
  void SetHostZoomLevel(const GURL& url, double zoom_level) override;

  // blink::WebLocalFrameClient implementation:
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  blink::WebMediaPlayer* CreateMediaPlayer(
      const blink::WebMediaPlayerSource& source,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      const blink::WebString& sink_id,
      blink::WebLayerTreeView* layer_tree_view) override;
  std::unique_ptr<blink::WebApplicationCacheHost> CreateApplicationCacheHost(
      blink::WebApplicationCacheHostClient* client) override;
  std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient() override;
  std::unique_ptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext()
      override;
  blink::WebExternalPopupMenu* CreateExternalPopupMenu(
      const blink::WebPopupMenuInfo& popup_menu_info,
      blink::WebExternalPopupMenuClient* popup_menu_client) override;
  blink::WebCookieJar* CookieJar() override;
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
      blink::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& container_policy,
      const blink::WebFrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType frame_owner_element_type) override;
  blink::WebFrame* FindFrame(const blink::WebString& name) override;
  void DidChangeOpener(blink::WebFrame* frame) override;
  void FrameDetached(DetachType type) override;
  void FrameFocused() override;
  void WillCommitProvisionalLoad() override;
  void DidChangeName(const blink::WebString& name) override;
  void DidEnforceInsecureRequestPolicy(
      blink::WebInsecureRequestPolicy policy) override;
  void DidEnforceInsecureNavigationsSet(
      const std::vector<uint32_t>& set) override;
  void DidChangeFramePolicy(
      blink::WebFrame* child_frame,
      blink::WebSandboxFlags flags,
      const blink::ParsedFeaturePolicy& container_policy) override;
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
                   CrossOriginRedirects cross_origin_redirect_behavior,
                   mojo::ScopedMessagePipeHandle blob_url_token) override;
  void LoadErrorPage(int reason) override;
  blink::WebNavigationPolicy DecidePolicyForNavigation(
      const NavigationPolicyInfo& info) override;
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void DidCreateDocumentLoader(
      blink::WebDocumentLoader* document_loader) override;
  void DidStartProvisionalLoad(
      blink::WebDocumentLoader* document_loader,
      blink::WebURLRequest& request,
      mojo::ScopedMessagePipeHandle navigation_initiator_handle) override;
  void DidFailProvisionalLoad(const blink::WebURLError& error,
                              blink::WebHistoryCommitType commit_type) override;
  void DidCommitProvisionalLoad(
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type,
      blink::WebGlobalObjectReusePolicy global_object_reuse_policy) override;
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
  void DidChangeThemeColor() override;
  void ForwardResourceTimingToParent(
      const blink::WebResourceTimingInfo& info) override;
  void DispatchLoad() override;
  blink::WebEffectiveConnectionType GetEffectiveConnectionType() override;
  void SetEffectiveConnectionTypeForTesting(
      blink::WebEffectiveConnectionType) override;
  blink::WebURLRequest::PreviewsState GetPreviewsStateForFrame() const override;
  void DidBlockFramebust(const blink::WebURL& url) override;
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
  bool RunFileChooser(
      const blink::WebFileChooserParams& params,
      blink::WebFileChooserCompletion* chooser_completion) override;
  void ShowContextMenu(const blink::WebContextMenuData& data) override;
  void SaveImageFromDataURL(const blink::WebString& data_url) override;
  void FrameRectsChanged(const blink::WebRect& frame_rect) override;
  void WillSendRequest(blink::WebURLRequest& request) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidLoadResourceFromMemoryCache(
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response) override;
  void DidDisplayInsecureContent() override;
  void DidContainInsecureFormAction() override;
  void DidRunInsecureContent(const blink::WebSecurityOrigin& origin,
                             const blink::WebURL& target) override;
  void DidDisplayContentWithCertificateErrors() override;
  void DidRunContentWithCertificateErrors() override;
  void ReportLegacySymantecCert(const blink::WebURL& url,
                                bool did_fail) override;
  void DidChangePerformanceTiming() override;
  void DidObserveLoadingBehavior(
      blink::WebLoadingBehaviorFlag behavior) override;
  void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature) override;
  void DidObserveNewCssPropertyUsage(int css_property,
                                     bool is_animated) override;
  void DidObserveLayoutJank(double jank_fraction) override;
  bool ShouldTrackUseCounter(const blink::WebURL& url) override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override;
  void DidChangeScrollOffset() override;
  blink::WebPushClient* PushClient() override;
  blink::WebRelatedAppsFetcher* GetRelatedAppsFetcher() override;
  void WillStartUsingPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandler* handler) override;
  blink::WebUserMediaClient* UserMediaClient() override;
  blink::WebEncryptedMediaClient* EncryptedMediaClient() override;
  blink::WebString UserAgentOverride() override;
  blink::WebString DoNotTrackValue() override;
  mojom::RendererAudioInputStreamFactory* GetAudioInputStreamFactory();
  bool ShouldBlockWebGL() override;
  bool AllowContentInitiatedDataUrlNavigations(
      const blink::WebURL& url) override;
  void PostAccessibilityEvent(const blink::WebAXObject& obj,
                              ax::mojom::Event event) override;
  void MarkWebAXObjectDirty(const blink::WebAXObject& obj,
                            bool subtree) override;
  void HandleAccessibilityFindInPageResult(int identifier,
                                           int match_index,
                                           const blink::WebNode& start_node,
                                           int start_offset,
                                           const blink::WebNode& end_node,
                                           int end_offset) override;
  void DidChangeManifest() override;
  void EnterFullscreen(const blink::WebFullscreenOptions& options) override;
  void ExitFullscreen() override;
  void FullscreenStateChanged(bool is_fullscreen) override;
  void SuddenTerminationDisablerChanged(
      bool present,
      blink::WebSuddenTerminationDisablerType disabler_type) override;
  void RegisterProtocolHandler(const blink::WebString& scheme,
                               const blink::WebURL& url,
                               const blink::WebString& title) override;
  void UnregisterProtocolHandler(const blink::WebString& scheme,
                                 const blink::WebURL& url) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      std::unique_ptr<blink::WebSetSinkIdCallbacks> callbacks) override;
  blink::mojom::PageVisibilityState VisibilityState() const override;
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
      blink::WebScrollGranularity granularity) override;

  // WebFrameSerializerClient implementation:
  void DidSerializeDataForFrame(
      const blink::WebVector<char>& data,
      blink::WebFrameSerializerClient::FrameSerializationStatus status)
      override;

  // Binds to the fullscreen service in the browser.
  void BindFullscreen(
      mojom::FullscreenVideoElementHandlerAssociatedRequest request);

  // Binds to the autoplay configuration service in the browser.
  void BindAutoplayConfiguration(
      blink::mojom::AutoplayConfigurationClientAssociatedRequest request);

  // Binds to the FrameHost in the browser.
  void BindFrame(const service_manager::BindSourceInfo& browser_info,
                 mojom::FrameRequest request);

  // Virtual so that a TestRenderFrame can mock out the interface.
  virtual mojom::FrameHost* GetFrameHost();

  void BindFrameBindingsControl(
      mojom::FrameBindingsControlAssociatedRequest request);
  void BindFrameNavigationControl(
      mojom::FrameNavigationControlAssociatedRequest request);
  // Only used when PerNavigationMojoInterface is enabled.
  void BindNavigationClient(mojom::NavigationClientAssociatedRequest request);

  blink::mojom::ManifestManager& GetManifestManager();

  media::MediaPermission* GetMediaPermission();

  // Sends the current frame's navigation state to the browser.
  void SendUpdateState();

  // Creates a MojoBindingsController if Mojo bindings have been enabled for
  // this frame. For WebUI, this allows the page to communicate with the browser
  // process; for layout tests, this allows the test to mock out services at
  // the Mojo IPC layer.
  void MaybeEnableMojoBindings();

  // Another RunFileChooser() for content::FileChooserParams.
  // Returns true if the chooser was successfully requested. False means we
  // didn't request anything.
  bool RunFileChooser(const blink::mojom::FileChooserParams& params,
                      blink::WebFileChooserCompletion* completion);

  // Internal version of DidFailProvisionalLoad() that allows specifying
  // |error_page_content|.
  void DidFailProvisionalLoadInternal(
      const blink::WebURLError& error,
      blink::WebHistoryCommitType commit_type,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::WebNavigationParams> navigation_params,
      std::unique_ptr<blink::WebDocumentLoader::ExtraData> document_state);

  void NotifyObserversOfFailedProvisionalLoad(const blink::WebURLError& error);

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

  const RendererPreferences& GetRendererPreferences() const;

#if defined(OS_MACOSX)
  void OnCopyToFindPboard();
#endif

  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed.
  // TODO(varunjain): delete this method once we figure out how to keep
  // selection handles in sync with the webpage.
  void SyncSelectionIfRequired();

  // Sets the custom URLLoaderFactory instance to be used for network requests.
  void SetCustomURLLoaderFactory(network::mojom::URLLoaderFactoryPtr factory);

  void ScrollFocusedEditableElementIntoRect(const gfx::Rect& rect);
  void DidChangeVisibleViewport();

  // Called to notify a frame that it called |window.focus()| on a different
  // frame.
  void FrameDidCallFocus();

  // Called when an ongoing renderer-initiated navigation was dropped by the
  // browser.
  void OnDroppedNavigation();

  void DidStartResponse(int request_id,
                        const network::ResourceResponseHead& response_head,
                        content::ResourceType resource_type);
  void DidCompleteResponse(int request_id,
                           const network::URLLoaderCompletionStatus& status);
  void DidCancelResponse(int request_id);
  void DidReceiveTransferSizeUpdate(int request_id, int received_data_length);

 protected:
  explicit RenderFrameImpl(CreateParams params);

 private:
  friend class RenderFrameImplTest;
  friend class RenderFrameObserver;
  friend class RenderAccessibilityImplTest;
  friend class TestRenderFrame;
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuDisplayNoneTest, SelectItem);
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
        int id,
        bool notify_result,
        int routing_id,
        base::WeakPtr<RenderFrameImpl> render_frame_impl);
    void Completed(
        const blink::WebVector<v8::Local<v8::Value>>& result) override;

   private:
    ~JavaScriptIsolatedWorldRequest() override;

    int id_;
    bool notify_result_;
    int routing_id_;
    base::WeakPtr<RenderFrameImpl> render_frame_impl_;

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

  typedef std::map<GURL, double> HostZoomLevels;

  // Creates a new RenderFrame. |render_view| is the RenderView object that this
  // frame belongs to, and |interface_provider| is the RenderFrameHost's
  // InterfaceProvider through which services are exposed to the RenderFrame.
  static RenderFrameImpl* Create(
      RenderViewImpl* render_view,
      int32_t routing_id,
      service_manager::mojom::InterfaceProviderPtr interface_provider,
      const base::UnguessableToken& devtools_frame_token);

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  bool IsLocalRoot() const;
  const RenderFrameImpl* GetLocalRoot() const;

  // Swaps the current frame into the frame tree, replacing the
  // RenderFrameProxy it is associated with.  Return value indicates whether
  // the swap operation succeeded.  This should only be used for provisional
  // frames associated with a proxy, while the proxy is still in the frame
  // tree.  If the associated proxy has been detached before this is called,
  // this returns false and aborts the swap.
  bool SwapIn();

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnBeforeUnload(bool is_reload);
  void OnSwapIn();
  void OnSwapOut(int proxy_routing_id,
                 bool is_loading,
                 const FrameReplicationState& replicated_frame_state);
  void OnDeleteFrame();
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
  void OnAddMessageToConsole(ConsoleMessageLevel level,
                             const std::string& message);
  void OnJavaScriptExecuteRequest(const base::string16& javascript,
                                  int id,
                                  bool notify_result);
  void OnJavaScriptExecuteRequestForTests(const base::string16& javascript,
                                          int id,
                                          bool notify_result,
                                          bool has_user_gesture);
  void OnJavaScriptExecuteRequestInIsolatedWorld(const base::string16& jscript,
                                                 int id,
                                                 bool notify_result,
                                                 int world_id);
  void OnVisualStateRequest(uint64_t key);
  void OnReload(bool bypass_cache);
  void OnReloadLoFiImages();
  void OnTextSurroundingSelectionRequest(uint32_t max_length);
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
  void OnCheckCompleted();
  void OnPostMessageEvent(FrameMsg_PostMessage_Params params);
  void OnReportContentSecurityPolicyViolation(
      const content::CSPViolationParams& violation_params);
  void OnGetSavableResourceLinks();
  void OnGetSerializedHtmlWithLocalLinks(
      const std::map<GURL, base::FilePath>& url_to_local_path,
      const std::map<int, base::FilePath>& frame_routing_id_to_local_path);
  void OnSerializeAsMHTML(const FrameMsg_SerializeAsMHTML_Params& params);
  void OnEnableViewSourceMode();
  void OnSuppressFurtherDialogs();
  void OnFileChooserResponse(
      const std::vector<blink::mojom::FileChooserFileInfoPtr>& files);
  void OnClearFocusedElement();
  void OnBlinkFeatureUsageReport(const std::set<int>& features);
  void OnMixedContentFound(const FrameMsg_MixedContentFound_Params& params);
  void OnSetOverlayRoutingToken(const base::UnguessableToken& token);
  void OnNotifyUserActivation();
  void OnMediaPlayerActionAt(const gfx::PointF&,
                             const blink::WebMediaPlayerAction&);
  void OnRenderFallbackContent() const;

  void PostMessageEvent(FrameMsg_PostMessage_Params params);

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  void OnSelectPopupMenuItem(int selected_index);
#else
  void OnSelectPopupMenuItems(bool canceled,
                              const std::vector<int>& selected_indices);
#endif
#endif

  // Callback scheduled from OnSerializeAsMHTML for when writing serialized
  // MHTML to file has been completed in the file thread.
  void OnWriteMHTMLToDiskComplete(
      int job_id,
      std::set<std::string> serialized_resources_uri_digests,
      base::TimeDelta main_thread_use_time,
      MhtmlSaveStatus save_status);

  // Requests that the browser process navigates to |url|. If
  // |is_history_navigation_in_new_child| is true, the browser process should
  // look for a matching FrameNavigationEntry in the last committed entry to use
  // instead of |url|.
  void OpenURL(const NavigationPolicyInfo& info,
               bool is_history_navigation_in_new_child);

  // Creates a WebURLRequest to use fo the commit of a navigation.
  blink::WebURLRequest CreateURLRequestForCommit(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      const network::ResourceResponseHead& head);

  // Returns a ChildURLLoaderFactoryBundle which can be used to request
  // subresources for this frame.
  // For frames with committed navigations, this bundle is created with the
  // factories provided by the browser at navigation time. For any other frames
  // (i.e. frames on the initial about:blank Document), the bundle returned here
  // is lazily cloned from the parent or opener's own bundle.
  ChildURLLoaderFactoryBundle* GetLoaderFactoryBundle();

  void SetupLoaderFactoryBundle(
      std::unique_ptr<URLLoaderFactoryBundleInfo> info,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      network::mojom::URLLoaderFactoryPtr prefetch_loader_factory);

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
  void UpdateEncoding(blink::WebFrame* frame,
                      const std::string& encoding_name);

  bool RunJavaScriptDialog(JavaScriptDialogType type,
                           const base::string16& message,
                           const base::string16& default_value,
                           base::string16* result);

  // Loads the appropriate error page for the specified failure into the frame.
  // |entry| is only when navigating to a history item.
  void LoadNavigationErrorPage(
      const blink::WebURLRequest& failed_request,
      const blink::WebURLError& error,
      bool replace,
      HistoryEntry* entry,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::WebNavigationParams> navigation_params,
      std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data);
  void LoadNavigationErrorPageForHttpStatusError(
      const blink::WebURLRequest& failed_request,
      const GURL& unreachable_url,
      int http_status,
      bool replace,
      HistoryEntry* entry,
      std::unique_ptr<blink::WebNavigationParams> navigation_params,
      std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data);
  void LoadNavigationErrorPageInternal(
      const std::string& error_html,
      const GURL& error_url,
      bool replace,
      HistoryEntry* history_entry,
      std::unique_ptr<blink::WebNavigationParams> navigation_params,
      std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data,
      const blink::WebURLRequest* failed_request);

  void HandleJavascriptExecutionResult(const base::string16& javascript,
                                       int id,
                                       bool notify_result,
                                       v8::Local<v8::Value> result);

  // Initializes |web_user_media_client_|. If this fails, because it wasn't
  // possible to create a MediaStreamClient (e.g., WebRTC is disabled), then
  // |web_user_media_client_| will remain NULL.
  void InitializeUserMediaClient();

  // Does preparation for the navigation to |url|.
  void PrepareRenderViewForNavigation(
      const GURL& url,
      const RequestNavigationParams& request_params);

  // Sends a FrameHostMsg_BeginNavigation to the browser
  void BeginNavigation(
      const NavigationPolicyInfo& info,
      mojo::ScopedMessagePipeHandle navigation_initiator_handle);

  // Loads a data url.
  void LoadDataURL(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      blink::WebFrameLoadType load_type,
      blink::WebHistoryItem item_for_history_navigation,
      bool is_client_redirect,
      std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data);

  // Sends a proper FrameHostMsg_DidFailProvisionalLoadWithError_Params IPC for
  // the failed request |request|.
  void SendFailedProvisionalLoad(const blink::WebURLRequest& request,
                                 const blink::WebURLError& error,
                                 blink::WebLocalFrame* frame);

  bool ShouldDisplayErrorPageForFailedLoad(int error_code,
                                           const GURL& unreachable_url);

  // Returns the URL being loaded by the |frame_|'s request.
  GURL GetLoadingUrl() const;

#if BUILDFLAG(ENABLE_PLUGINS)
  void HandlePepperImeCommit(const base::string16& text);
#endif  // ENABLE_PLUGINS

  void RegisterMojoInterfaces();

  void OnHostZoomClientRequest(mojom::HostZoomAssociatedRequest request);

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

  void BindWidget(mojom::WidgetRequest request);

  void ShowDeferredContextMenu(const ContextMenuParams& params);

  // Whether the frame is controlled by a service worker.
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker();

  // Build DidCommitProvisionalLoad_Params based on the frame internal state.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  MakeDidCommitProvisionalLoadParams(blink::WebHistoryCommitType commit_type,
                                     ui::PageTransition transition);

  // Updates the Zoom level of the render view to match current content.
  void UpdateZoomLevel();

  // Updates the navigation history depending on the passed parameters.
  // This could result either in the creation of a new entry or a modification
  // of the current entry or nothing. If a new entry was created,
  // returns true, false otherwise.
  bool UpdateNavigationHistory(const blink::WebHistoryItem& item,
                               blink::WebHistoryCommitType commit_type);

  // Notify render_view_ observers that a commit happened.
  void NotifyObserversOfNavigationCommit(bool is_new_navigation,
                                         bool is_same_document,
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
      service_manager::mojom::InterfaceProviderRequest
          remote_interface_provider_request);

  blink::WebComputedAXTree* GetOrCreateWebComputedAXTree() override;

  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() override;
  bool IsPluginHandledExternally(
      const blink::WebElement& plugin_element,
      const blink::WebURL& url,
      const blink::WebString& suggested_mime_type) override;

  // Updates the state of this frame when asked to commit a navigation.
  void PrepareFrameForCommit(const GURL& url,
                             const RequestNavigationParams& request_params);

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
      FrameMsg_Navigate_Type::Value navigation_type,
      const RequestNavigationParams& request_params,
      blink::WebHistoryItem* item_for_history_navigation,
      blink::WebFrameLoadType* load_type);

  // Whether url download should be throttled.
  bool ShouldThrottleDownload();

  // Creates a service worker network provider using browser provided data,
  // to be supplied to the loader.
  std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  BuildServiceWorkerNetworkProviderForNavigation(
      const RequestNavigationParams* request_params,
      mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info);

  // Stores the WebLocalFrame we are associated with.  This is null from the
  // constructor until BindToFrame() is called, and it is null after
  // FrameDetached() is called until destruction (which is asynchronous in the
  // case of the main frame, but not subframes).
  blink::WebLocalFrame* frame_;

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

  // When a frame is detached in response to a message from the browser process,
  // this RenderFrame should not be sending notifications back to it. This
  // boolean is used to indicate this case.
  bool in_browser_initiated_detach_;

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

  // If this frame was created to replace a proxy, this will store the routing
  // id of the proxy to replace at commit-time, at which time it will be
  // cleared.
  // TODO(creis): Remove this after switching to PlzNavigate.
  int proxy_routing_id_;

  // Non-null when the RenderFrame is a local root for compositing, input,
  // layout, etc. A local frame is also a local root iff it does not have a
  // parent that is a local frame.
  scoped_refptr<RenderWidget> render_widget_;

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
  std::map<std::string, bool> history_subframe_unique_names_;

  // Stores the current history item for this frame, so that updates to it can
  // be reported to the browser process via SendUpdateState.
  blink::WebHistoryItem current_history_item_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Current text input composition text. Empty if no composition is in
  // progress.
  base::string16 pepper_composition_text_;

  PluginPowerSaverHelper* plugin_power_saver_helper_;
#endif

  RendererWebCookieJarImpl cookie_jar_;

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

  // The next group of objects all implement RenderFrameObserver, so are deleted
  // along with the RenderFrame automatically.  This is why we just store weak
  // references.

  // Destroyed via the RenderFrameObserver::OnDestruct() mechanism.
  UserMediaClientImpl* web_user_media_client_;

  mojom::RendererAudioInputStreamFactoryPtr audio_input_stream_factory_;

  // The media permission dispatcher attached to this frame.
  std::unique_ptr<MediaPermissionDispatcher> media_permission_dispatcher_;

  // The PushMessagingClient attached to this frame, lazily initialized.
  PushMessagingClient* push_messaging_client_;

  service_manager::BinderRegistry registry_;
  service_manager::InterfaceProvider remote_interfaces_;
  std::unique_ptr<BlinkInterfaceRegistryImpl> blink_interface_registry_;

  service_manager::BindSourceInfo local_info_;
  service_manager::BindSourceInfo remote_info_;

  // The Connector proxy used to connect to services.
  service_manager::mojom::ConnectorPtr connector_;

  // The Manifest Manager handles the manifest requests from the browser
  // process.
  std::unique_ptr<ManifestManager> manifest_manager_;

  // The current accessibility mode.
  ui::AXMode accessibility_mode_;

  // Only valid if |accessibility_mode_| has |ui::AXMode::kWebContents|
  // flag set.
  RenderAccessibilityImpl* render_accessibility_;

  std::unique_ptr<RelatedAppsFetcher> related_apps_fetcher_;

  // The PreviewsState of this RenderFrame that indicates which Previews can
  // be used. The PreviewsState is a bitmask of potentially several Previews
  // optimizations.
  // TODO(sclittle): Consider moving this into Blink to be owned and managed by
  // LocalFrame or another class around there.
  PreviewsState previews_state_;

  // Effective connection type when the document of this frame was fetched.
  // TODO(sclittle): Consider moving this into Blink to be owned and managed by
  // LocalFrame or another class around there.
  blink::WebEffectiveConnectionType effective_connection_type_;

  // Whether or not this RenderFrame is currently pasting.
  bool is_pasting_;

  // Whether we must stop creating nested run loops for modal dialogs. This
  // is necessary because modal dialogs have a ScopedPageLoadDeferrer on the
  // stack that interferes with swapping out.
  bool suppress_further_dialogs_;

  // The current file chooser completion object.
  blink::WebFileChooserCompletion* file_chooser_completion_ = nullptr;

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // The external popup for the currently showing select popup.
  std::unique_ptr<ExternalPopupMenu> external_popup_menu_;
#endif

  std::unique_ptr<FrameBlameContext> blame_context_;
  std::unique_ptr<SharedWorkerRepository> shared_worker_repository_;

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

  HostZoomLevels host_zoom_levels_;

  using AutoplayOriginAndFlags = std::pair<url::Origin, int32_t>;
  AutoplayOriginAndFlags autoplay_flags_;

  mojo::AssociatedBinding<blink::mojom::AutoplayConfigurationClient>
      autoplay_configuration_binding_;
  mojo::Binding<mojom::Frame> frame_binding_;
  mojo::AssociatedBinding<mojom::HostZoom> host_zoom_binding_;
  mojo::AssociatedBinding<mojom::FrameBindingsControl>
      frame_bindings_control_binding_;
  mojo::AssociatedBinding<mojom::FrameNavigationControl>
      frame_navigation_control_binding_;
  mojo::AssociatedBinding<mojom::FullscreenVideoElementHandler>
      fullscreen_binding_;

  // Only used when PerNavigationMojoInterface is enabled.
  std::unique_ptr<NavigationClient> navigation_client_impl_;

  // Indicates whether |didAccessInitialDocument| was called.
  bool has_accessed_initial_document_;

  // Creates various media clients.
  MediaFactory media_factory_;

  blink::AssociatedInterfaceRegistry associated_interfaces_;
  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // TODO(dcheng): Remove these members.
  bool committed_first_load_ = false;
  bool name_changed_before_first_commit_ = false;

  bool browser_side_navigation_pending_ = false;
  GURL browser_side_navigation_pending_url_;

  // A bitwise OR of bindings types that have been enabled for this RenderFrame.
  // See BindingsPolicy for details.
  int enabled_bindings_ = 0;

  // Contains information about a pending navigation to be sent to the browser.
  // We save information about the navigation in decidePolicyForNavigation().
  // The navigation is sent to the browser in didStartProvisionalLoad().
  // Please see the BeginNavigation() for information.
  struct PendingNavigationInfo {
    blink::WebNavigationType navigation_type;
    blink::WebNavigationPolicy policy;
    bool replaces_current_history_item;
    bool history_navigation_in_new_child_frame;
    bool client_redirect;
    blink::WebTriggeringEventInfo triggering_event_info;
    blink::WebFormElement form;
    blink::WebSourceLocation source_location;
    blink::WebString devtools_initiator_info;
    blink::mojom::BlobURLTokenPtr blob_url_token;
    base::TimeTicks input_start;

    explicit PendingNavigationInfo(const NavigationPolicyInfo& info);
    ~PendingNavigationInfo();
  };

  // Contains information about a pending navigation to be sent to the browser.
  // This state is allocated in decidePolicyForNavigation() and is used and
  // released in didStartProvisionalLoad().
  std::unique_ptr<PendingNavigationInfo> pending_navigation_info_;

  service_manager::BindSourceInfo browser_info_;

  mojom::FrameHostAssociatedPtr frame_host_ptr_;
  mojo::BindingSet<service_manager::mojom::InterfaceProvider>
      interface_provider_bindings_;

  // URLLoaderFactory instances used for subresource loading.
  // Depending on how the frame was created, |loader_factories_| could be:
  //   * |HostChildURLLoaderFactoryBundle| for standalone frames, or
  //   * |TrackedChildURLLoaderFactoryBundle| for frames opened by other frames.
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
  blink::mojom::ClipboardHostPtr clipboard_host_;
#endif

  // Used to cap the number of console messages that are printed to warn about
  // legacy certificates that will be distrusted in future or have already been
  // distrusted.
  uint32_t num_certificate_warning_messages_ = 0;
  // The origins for which a legacy certificate warning has been printed.
  std::set<url::Origin> certificate_warning_origins_;

  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  // Variable to control burst of download requests.
  int num_burst_download_requests_ = 0;
  base::TimeTicks burst_download_start_time_;

  // Set to true while we are committing a navigation and
  // main request is being issued (the one which already got
  // a response).
  // TODO(dgozman): should be temporary until we stop using
  // WebURLRequest for this.
  bool committing_main_request_ = false;

  // Set to true while we are replaying main resource response,
  // which was captured in the browser, during navigation commit.
  // TODO(dgozman): should be temporary until we stop using
  // WebURLRequest for this.
  bool replaying_main_response_ = false;

  base::WeakPtrFactory<RenderFrameImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
