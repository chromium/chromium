// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/download/mhtml_file_writer.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/common/navigation_client.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/web_ui.mojom.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/extra_mojo_js_features.mojom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/widget_type.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "content/renderer/content_security_policy_util.h"
#include "content/renderer/media/media_factory.h"
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
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/permissions_policy/document_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/triggering_event_info.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info_notifier.mojom.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/child_url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_thread_safe_data.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-forward.h"

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/common/pepper_plugin.mojom.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/common/gin_java_bridge.mojom.h"
#endif

namespace blink {
namespace scheduler {
class WebAgentGroupScheduler;
}  // namespace scheduler

class WeakWrapperResourceLoadInfoNotifier;
class WebBackgroundResourceFetchAssets;
class WebComputedAXTree;
class WebContentDecryptionModule;
class WebElement;
class WebLocalFrame;
class WebMediaStreamDeviceObserver;
class WebString;
class WebURL;
struct FramePolicy;
struct JavaScriptFrameworkDetectionResult;
struct SoftNavigationMetrics;
}  // namespace blink

namespace gfx {
class Point;
class Range;
}  // namespace gfx

namespace media {
class MediaPermission;
}

namespace url {
class Origin;
class SchemeHostPort;
}  // namespace url

namespace content {

class AgentSchedulingGroup;
class BlinkInterfaceRegistryImpl;
class DocumentState;
class MediaPermissionDispatcher;
class MHTMLPartsGenerationDelegateImpl;
class NavigationClient;
class PepperPluginInstanceImpl;
class RendererPpapiHost;
class RenderAccessibilityManager;
class RenderFrameObserver;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      public blink::mojom::ResourceLoadInfoNotifier,
      blink::mojom::AutoplayConfigurationClient,
      public mojom::Frame,
      mojom::FrameBindingsControl,
      mojom::MhtmlFileWriter,
      public blink::WebLocalFrameClient,
      service_manager::mojom::InterfaceProvider {
 public:
  // Creates a new RenderFrame as the main frame of `web_view`. Note that not
  // all main RenderFrame creation uses this function. `CreateMainFrame()`
  // is used to create a RenderFrame that is immediately attached as the main
  // frame of the `web_view`. Meanwhile, `CreateFrame()` is used to create
  // provisional main RenderFrames (and subframe creation cases).
  static RenderFrameImpl* CreateMainFrame(
      AgentSchedulingGroup& agent_scheduling_group,
      blink::WebView* web_view,
      blink::WebFrame* opener,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page,
      blink::mojom::FrameReplicationStatePtr replication_state,
      const base::UnguessableToken& devtools_frame_token,
      mojom::CreateLocalMainFrameParamsPtr params,
      const blink::WebURL& base_url);

  // Creates a new RenderFrame with |routing_id|. If |previous_frame_token| is
  // not provided, it creates the Blink WebLocalFrame and inserts it into
  // the frame tree after the frame identified by |previous_sibling_routing_id|,
  // or as the first child if |previous_sibling_routing_id| is MSG_ROUTING_NONE.
  // Otherwise, the frame is semi-orphaned until it commits, at which point it
  // replaces the previous object identified by |previous_frame_token|. The
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
  // The |web_view| param will only be set when the frame to be created will use
  // new WebView, instead of using the previous Frame's WebView. This is only
  // possible for provisional main RenderFrames that will do a local main
  // RenderFrame swap later on with the frame that has the token
  // |previous_frame_token|.
  //
  // Note: This is called only when RenderFrame is being created in response
  // to IPC message from the browser process. All other frame creation is driven
  // through Blink and Create.
  static void CreateFrame(
      AgentSchedulingGroup& agent_scheduling_group,
      const blink::LocalFrameToken& token,
      int routing_id,
      mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
          associated_interface_provider,
      blink::WebView* web_view,
      const std::optional<blink::FrameToken>& previous_frame_token,
      const std::optional<blink::FrameToken>& opener_frame_token,
      const std::optional<blink::FrameToken>& parent_frame_token,
      const std::optional<blink::FrameToken>& previous_sibling_frame_token,
      const base::UnguessableToken& devtools_frame_token,
      blink::mojom::TreeScopeType tree_scope_type,
      blink::mojom::FrameReplicationStatePtr replicated_state,
      mojom::CreateFrameWidgetParamsPtr widget_params,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
      bool is_on_initial_empty_document,
      const blink::DocumentToken& document_token,
      blink::mojom::PolicyContainerPtr policy_container,
      bool is_for_nested_main_frame);

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // Returns the RenderFrameImpl for the given routing ID.
  static RenderFrameImpl* FromRoutingID(int routing_id);
#endif

  // Just like RenderFrame::FromWebFrame but returns the implementation.
  static RenderFrameImpl* FromWebFrame(blink::WebFrame* web_frame);

  // Constructor parameters are bundled into a struct.
  struct CONTENT_EXPORT CreateParams {
    CreateParams(
        AgentSchedulingGroup& agent_scheduling_group,
        const blink::LocalFrameToken& frame_token,
        int32_t routing_id,
        mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver,
        mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
            associated_interface_provider,
        const base::UnguessableToken& devtools_frame_token,
        bool is_for_nested_main_frame);
    ~CreateParams();

    CreateParams(CreateParams&&);
    CreateParams& operator=(CreateParams&&);

    raw_ptr<AgentSchedulingGroup> agent_scheduling_group;
    blink::LocalFrameToken frame_token;
    int32_t routing_id;
    mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver;
    mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        associated_interface_provider;
    base::UnguessableToken devtools_frame_token;
    bool is_for_nested_main_frame;
  };

  using CreateRenderFrameImplFunction = RenderFrameImpl* (*)(CreateParams);

  // Web tests override the creation of RenderFrames in order to inject a
  // partial testing fake.
  static void InstallCreateHook(CreateRenderFrameImplFunction create_frame);

  // Overwrites the given URL to use an HTML5 embed if possible.
  blink::WebURL OverrideFlashEmbedWithHTML(const blink::WebURL& url) override;

  RenderFrameImpl(const RenderFrameImpl&) = delete;
  RenderFrameImpl& operator=(const RenderFrameImpl&) = delete;

  ~RenderFrameImpl() override;

  // Returns the unique name of the RenderFrame.
  const std::string& unique_name() const { return unique_name_helper_.value(); }

  // Returns the blink::WebFrameWidget attached to the local root of this
  // frame.
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
  // TODO(crbug.com/40452626): Remove this once provisional frames are
  // gone, and clean up code that depends on it.
  bool in_frame_tree() { return in_frame_tree_; }

#if BUILDFLAG(ENABLE_PPAPI)
  mojom::PepperHost* GetPepperHost();

  // Notification that a PPAPI plugin has been created.
  void PepperPluginCreated(RendererPpapiHost* host);

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PepperTextInputTypeChanged(PepperPluginInstanceImpl* instance);
  void PepperCaretPositionChanged(PepperPluginInstanceImpl* instance);

  // Cancels current composition.
  void PepperCancelComposition(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed selection.
  void PepperSelectionChanged(PepperPluginInstanceImpl* instance);

#endif  // BUILDFLAG(ENABLE_PPAPI)

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

#define LEGACY_IPC_OVERRIDE override
#else
#define LEGACY_IPC_OVERRIDE
#endif

  void OnAssociatedInterfaceRequest(const std::string& interface_name,
                                    mojo::ScopedInterfaceEndpointHandle handle)
      LEGACY_IPC_OVERRIDE;

#undef LEGACY_IPC_OVERRIDE

  // RenderFrame implementation:
  RenderFrame* GetMainRenderFrame() override;
  RenderAccessibility* GetRenderAccessibility() override;
  std::unique_ptr<AXTreeSnapshotter> CreateAXTreeSnapshotter(
      ui::AXMode ax_mode) override;
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  int GetRoutingID() override;
#endif
  blink::WebLocalFrame* GetWebFrame() override;
  const blink::WebLocalFrame* GetWebFrame() const override;
  blink::WebView* GetWebView() override;
  const blink::WebView* GetWebView() const override;
  const blink::web_pref::WebPreferences& GetBlinkPreferences() override;
  void ShowVirtualKeyboard() override;
  blink::WebPlugin* CreatePlugin(const WebPluginInfo& info,
                                 const blink::WebPluginParams& params) override;
  void ExecuteJavaScript(const std::u16string& javascript) override;
  bool IsMainFrame() override;
  bool IsInFencedFrameTree() const override;
  bool IsHidden() override;
  void BindLocalInterface(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  blink::AssociatedInterfaceRegistry* GetAssociatedInterfaceRegistry() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  void SetSelectedText(const std::u16string& selection_text,
                       size_t offset,
                       const gfx::Range& range) override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  bool IsPasting() override;
  bool IsRequestingNavigation() override;
  void LoadHTMLStringForTesting(std::string_view html,
                                const GURL& base_url,
                                const std::string& text_encoding,
                                const GURL& unreachable_url,
                                bool replace_current_item) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      blink::TaskType task_type) override;
  BindingsPolicySet GetEnabledBindings() override;
  void SetAccessibilityModeForTest(ui::AXMode new_mode) override;
  const RenderFrameMediaPlaybackOptions& GetRenderFrameMediaPlaybackOptions()
      override;
  void SetRenderFrameMediaPlaybackOptions(
      const RenderFrameMediaPlaybackOptions& opts) override;
  void SetAllowsCrossBrowsingInstanceFrameLookup() override;
  [[nodiscard]] gfx::Rect ConvertViewportToWindow(
      const gfx::Rect& rect) override;
  float GetDeviceScaleFactor() override;
  blink::scheduler::WebAgentGroupScheduler& GetAgentGroupScheduler() override;

  // blink::mojom::AutoplayConfigurationClient implementation:
  void AddAutoplayFlags(const url::Origin& origin,
                        const int32_t flags) override;

  // blink::mojom::ResourceLoadInfoNotifier implementation:
#if BUILDFLAG(IS_ANDROID)
  void NotifyUpdateUserGestureCarryoverInfo() override;
#endif
  void NotifyResourceRedirectReceived(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr redirect_response) override;
  void NotifyResourceResponseReceived(
      int64_t request_id,
      const url::SchemeHostPort& final_response_url,
      network::mojom::URLResponseHeadPtr head,
      network::mojom::RequestDestination request_destination,
      bool is_ad_resource) override;
  void NotifyResourceTransferSizeUpdated(int64_t request_id,
                                         int32_t transfer_size_diff) override;
  void NotifyResourceLoadCompleted(
      blink::mojom::ResourceLoadInfoPtr resource_load_info,
      const ::network::URLLoaderCompletionStatus& status) override;
  void NotifyResourceLoadCanceled(int64_t request_id) override;
  void Clone(mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier>
                 pending_resource_load_info_notifier) override;

  // mojom::FrameBindingsControl implementation:
  void AllowBindings(int64_t enabled_bindings_flags) override;
  void EnableMojoJsBindings(
      content::mojom::ExtraMojoJsFeaturesPtr features) override;
  void EnableMojoJsBindingsWithBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>) override;
  void BindWebUI(
      mojo::PendingAssociatedReceiver<mojom::WebUI> Receiver,
      mojo::PendingAssociatedRemote<mojom::WebUIHost> remote) override;

  // These mirror mojom::NavigationClient, called by NavigationClient.
  void CommitNavigation(
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
      const blink::DocumentToken& document_token,
      const base::UnguessableToken& devtools_navigation_token,
      const base::Uuid& base_auction_nonce,
      const std::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
      mojo::PendingRemote<blink::mojom::CodeCacheHost>
          code_cache_host_for_background,
      mojom::CookieManagerInfoPtr cookie_manager_info,
      mojom::StorageInfoPtr storage_info,
      mojom::NavigationClient::CommitNavigationCallback commit_callback);
  void CommitFailedNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      int extended_error_code,
      net::ResolveErrorInfo resolve_error_info,
      const std::optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      const blink::DocumentToken& document_token,
      blink::mojom::PolicyContainerPtr policy_container,
      mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
      mojom::NavigationClient::CommitFailedNavigationCallback
          per_navigation_mojo_interface_callback);

  // mojom::MhtmlFileWriter implementation:
  void SerializeAsMHTML(const mojom::SerializeAsMHTMLParamsPtr params,
                        SerializeAsMHTMLCallback callback) override;

  // blink::WebLocalFrameClient implementation:
  void BindToFrame(blink::WebNavigationControl* frame) override;
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  std::unique_ptr<blink::WebMediaPlayer> CreateMediaPlayer(
      const blink::WebMediaPlayerSource& source,
      blink::WebMediaPlayerClient* client,
      blink::MediaInspectorContext* inspector_context,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      const blink::WebString& sink_id,
      const cc::LayerTreeSettings* settings,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner) override;
  std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient() override;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<media::SpeechRecognitionClient>
  CreateSpeechRecognitionClient() override;
#endif
  scoped_refptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext()
      override;
  scoped_refptr<blink::WebWorkerFetchContext>
  CreateWorkerFetchContextForPlzDedicatedWorker(
      blink::WebDedicatedWorkerHostFactoryClient* factory_client) override;
  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking()
      override;
  std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
  CreateResourceLoadInfoNotifierWrapper() override;
  std::unique_ptr<blink::WebServiceWorkerProvider> CreateServiceWorkerProvider()
      override;
  blink::AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces()
      override;
  blink::WebLocalFrame* CreateChildFrame(
      blink::mojom::TreeScopeType scope,
      const blink::WebString& name,
      const blink::WebString& fallback_name,
      const blink::FramePolicy& frame_policy,
      const blink::WebFrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType frame_owner_element_type,
      blink::WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override;
  void DidCreateFencedFrame(
      const blink::RemoteFrameToken& frame_token) override;
  blink::WebFrame* FindFrame(const blink::WebString& name) override;
  void WillDetach(blink::DetachReason detach_reason) override;
  void FrameDetached() override;
  void DidChangeName(const blink::WebString& name) override;
  void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
      override;
  bool ShouldReportDetailedMessageForSourceAndSeverity(
      blink::mojom::ConsoleMessageLevel log_level,
      const blink::WebString& source) override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;
  void DidCreateDocumentLoader(
      blink::WebDocumentLoader* document_loader) override;
  bool SwapIn(blink::WebFrame* previous_web_frame) override;
  void DidCommitNavigation(
      blink::WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker,
      const blink::ParsedPermissionsPolicy& permissions_policy_header,
      const blink::DocumentPolicyFeatureState& document_policy_header) override;
  void DidCommitDocumentReplacementNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidClearWindowObject() override;
  void DidCreateDocumentElement() override;
  void RunScriptsAtDocumentElementAvailable() override;
  void DidReceiveTitle(const blink::WebString& title) override;
  void DidDispatchDOMContentLoadedEvent() override;
  void RunScriptsAtDocumentReady() override;
  void RunScriptsAtDocumentIdle() override;
  void DidHandleOnloadEvents() override;
  void DidFinishLoad() override;
  void DidFinishLoadForPrinting() override;
  void DidFinishSameDocumentNavigation(
      blink::WebHistoryCommitType commit_type,
      bool is_synchronously_committed,
      blink::mojom::SameDocumentNavigationType same_document_navigation_type,
      bool is_client_redirect,
      const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
          screenshot_destination) override;
  void DidFailAsyncSameDocumentCommit() override;
  void WillFreezePage() override;
  void DidOpenDocumentInputStream(const blink::WebURL& url) override;
  void DidSetPageLifecycleState(bool restoring_from_bfcache) override;
  void NotifyCurrentHistoryItemChanged() override;
  void DidUpdateCurrentHistoryItem() override;
  base::UnguessableToken GetDevToolsFrameToken() override;
  void AbortClientNavigation(bool for_new_navigation) override;
  void DidChangeSelection(bool is_empty_selection,
                          blink::SyncCondition force_sync) override;
  void FocusedElementChanged(const blink::WebElement& element) override;
  void OnMainFrameIntersectionChanged(
      const gfx::Rect& main_frame_intersection_rect) override;
  void OnMainFrameViewportRectangleChanged(
      const gfx::Rect& main_frame_viewport_rect) override;
  void OnMainFrameImageAdRectangleChanged(
      int element_id,
      const gfx::Rect& image_ad_rect) override;
  void FinalizeRequest(blink::WebURLRequest& request) override;
  std::optional<blink::WebURL> WillSendRequest(
      const blink::WebURL& target,
      const blink::WebSecurityOrigin& security_origin,
      const net::SiteForCookies& site_for_cookies,
      ForRedirect for_redirect,
      const blink::WebURL& upstream_url) override;
  void OnOverlayPopupAdDetected() override;
  void OnLargeStickyAdDetected() override;
  void DidLoadResourceFromMemoryCache(
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response) override;
  void DidChangePerformanceTiming() override;
  void DidObserveUserInteraction(base::TimeTicks max_event_start,
                                 base::TimeTicks max_event_queued_main_thread,
                                 base::TimeTicks max_event_commit_finish,
                                 base::TimeTicks max_event_end,
                                 blink::UserInteractionType interaction_type,
                                 uint64_t interaction_offset) override;
  void DidChangeCpuTiming(base::TimeDelta time) override;
  void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior) override;
  void DidObserveJavaScriptFrameworks(
      const blink::JavaScriptFrameworkDetectionResult&) override;
  void DidObserveSubresourceLoad(
      const blink::SubresourceLoadMetrics& subresource_load_metrics) override;
  void DidObserveNewFeatureUsage(
      const blink::UseCounterFeature& feature) override;
  void DidObserveSoftNavigation(blink::SoftNavigationMetrics metrics) override;
  void DidObserveLayoutShift(double score, bool after_input_or_scroll) override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override;
  void DidChangeScrollOffset() override;
  blink::WebMediaStreamDeviceObserver* MediaStreamDeviceObserver() override;
  blink::WebEncryptedMediaClient* EncryptedMediaClient() override;
  blink::WebString UserAgentOverride() override;
  std::optional<blink::UserAgentMetadata> UserAgentMetadataOverride() override;
  blink::mojom::RendererAudioInputStreamFactory* GetAudioInputStreamFactory();
  bool AllowContentInitiatedDataUrlNavigations(
      const blink::WebURL& url) override;
  void PostAccessibilityEvent(const ui::AXEvent& event) override;
  bool SendAccessibilitySerialization(
      std::vector<ui::AXTreeUpdate> updates,
      std::vector<ui::AXEvent> events,
      ui::AXLocationAndScrollUpdates location_and_scroll_updates,
      bool had_load_complete_messages) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      blink::WebSetSinkIdCompleteCallback callback) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  blink::URLLoaderThrottleProvider* GetURLLoaderThrottleProvider() override;
  scoped_refptr<blink::WebBackgroundResourceFetchAssets>
  MaybeGetBackgroundResourceFetchAssets() override;
  void OnStopLoading() override;
  const blink::BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker()
      override;
  blink::WebView* CreateNewWindow(
      const blink::WebURLRequest& request,
      const blink::WebWindowFeatures& features,
      const blink::WebString& frame_name,
      blink::WebNavigationPolicy policy,
      network::mojom::WebSandboxFlags sandbox_flags,
      const blink::SessionStorageNamespaceId& session_storage_namespace_id,
      bool& consumed_user_gesture,
      const std::optional<blink::Impression>& impression,
      const std::optional<blink::WebPictureInPictureWindowOptions>& pip_options,
      const blink::WebURL& base_url) override;
  std::unique_ptr<blink::WebLinkPreviewTriggerer> CreateLinkPreviewTriggerer()
      override;

  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed or if the forced flag is passed. The forced flag is used
  // when the browser selection may be out of sync with the renderer due to
  // incorrect prediction.
  void SyncSelectionIfRequired(blink::SyncCondition force_sync) override;
  void CreateAudioInputStream(
      blink::CrossVariantMojoRemote<
          blink::mojom::RendererAudioInputStreamFactoryClientInterfaceBase>
          client,
      const base::UnguessableToken& session_id,
      const media::AudioParameters& params,
      bool automatic_gain_control,
      uint32_t shared_memory_count,
      blink::CrossVariantMojoReceiver<
          media::mojom::AudioProcessorControlsInterfaceBase> controls_receiver,
      const media::AudioProcessingSettings* settings) override;
  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void DidCommitAndDrawCompositorFrame() override;
  void WasHidden() override;
  void WasShown() override;
  void OnFrameVisibilityChanged(
      blink::mojom::FrameVisibility render_status) override;

  void SetUpSharedMemoryForSmoothness(
      base::ReadOnlySharedMemoryRegion shared_memory) override;
  blink::WebURL LastCommittedUrlForUKM() override;
  void ScriptedPrint() override;

  // Possibly defers the loading of media resources.
  //
  // This function defers in two cases:
  // - In the normal case, it calls ContentRendererClient::DeferMediaLoad()
  //   to give the embedder a chance to defer.
  // - If the frame is prerendering, this function defers the load. It
  //   calls ContentRendererClient::DeferMediaLoad() once activation
  //   occurs.
  //
  // `closure` is run when loading should proceed. Returns true if running
  // of |closure| is deferred; false if run immediately.
  //
  // If `has_played_media_before` is true, the render frame has previously
  // started media playback (i.e. played audio and video).
  bool DeferMediaLoad(bool has_played_media_before, base::OnceClosure closure);

  // Binds to the MHTML file generation service in the browser.
  void BindMhtmlFileWriter(
      mojo::PendingAssociatedReceiver<mojom::MhtmlFileWriter> receiver);

#if BUILDFLAG(IS_ANDROID)
  void BindGinJavaBridge(
      mojo::PendingAssociatedReceiver<mojom::GinJavaBridge> receiver);
#endif

  // Binds to the autoplay configuration service in the browser.
  void BindAutoplayConfiguration(
      mojo::PendingAssociatedReceiver<blink::mojom::AutoplayConfigurationClient>
          receiver);

  void BindFrameBindingsControl(
      mojo::PendingAssociatedReceiver<mojom::FrameBindingsControl> receiver);
  void BindNavigationClient(
      mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver);
  void BindNavigationClientWithParams(
      mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      blink::mojom::CommonNavigationParamsPtr common_params,
      bool is_duplicate_navigation);

  // Virtual so that a TestRenderFrame can mock out the interface.
  virtual mojom::FrameHost* GetFrameHost();

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

#if BUILDFLAG(ENABLE_PPAPI)
  PepperPluginInstanceImpl* focused_pepper_plugin() {
    return focused_pepper_plugin_;
  }
  // Indicates that the given instance has been created.
  void PepperInstanceCreated(
      PepperPluginInstanceImpl* instance,
      mojo::PendingAssociatedRemote<mojom::PepperPluginInstance> mojo_instance,
      mojo::PendingAssociatedReceiver<mojom::PepperPluginInstanceHost>
          mojo_host);

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  void PepperInstanceDeleted(PepperPluginInstanceImpl* instance);

  // Notification that the given plugin is focused or unfocused.
  void PepperFocusChanged(PepperPluginInstanceImpl* instance, bool focused);

  void OnSetPepperVolume(int32_t pp_instance, double volume);
#endif  // BUILDFLAG(ENABLE_PPAPI)

  const blink::RendererPreferences& GetRendererPreferences() const;

  // Called when an ongoing renderer-initiated navigation was dropped by the
  // browser.
  void OnDroppedNavigation();

  void DidStartResponse(const url::SchemeHostPort& final_response_url,
                        int request_id,
                        network::mojom::URLResponseHeadPtr response_head,
                        network::mojom::RequestDestination request_destination,
                        bool is_ad_resource);
  void DidCompleteResponse(int request_id,
                           const network::URLLoaderCompletionStatus& status);
  void DidCancelResponse(int request_id);
  void DidReceiveTransferSizeUpdate(int request_id, int received_data_length);

  bool GetCaretBoundsFromFocusedPlugin(gfx::Rect& rect) override;

  // Used in tests to install a fake URLLoaderFactory via
  // RenderViewTest::CreateFakeURLLoaderFactory().
  void SetURLLoaderFactoryOverrideForTest(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  // Clones and returns `this` frame's blink::ChildURLLoaderFactoryBundle.
  scoped_refptr<blink::ChildURLLoaderFactoryBundle> CloneLoaderFactories();

  url::Origin GetSecurityOriginOfTopFrame();

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  base::WeakPtr<media::DecoderFactory> GetMediaDecoderFactory();

  // Returns a blink::ChildURLLoaderFactoryBundle which can be used to request
  // subresources for this frame.
  //
  // The returned bundle was typically sent by the browser process when
  // committing a navigation, but in some cases (about:srcdoc, initial empty
  // document) it may be inherited from the parent or opener.
  blink::ChildURLLoaderFactoryBundle* GetLoaderFactoryBundle() override;

 protected:
  explicit RenderFrameImpl(CreateParams params);

  bool IsLocalRoot() const;
  const RenderFrameImpl* GetLocalRoot() const;

  base::WeakPtr<RenderFrameImpl> GetWeakPtr();

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
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplTest, SendUpdateCancelsPending);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplMojoJsDeathTest,
                           EnabledBindingsTampered);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplMojoJsDeathTest,
                           EnableMojoJsBindingsTampered);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameImplMojoJsDeathTest,
                           MojoJsInterfaceBrokerTampered);

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
    raw_ptr<T> scoped_variable_;
    T original_value_;
  };

  // Creates a new RenderFrame.
  static RenderFrameImpl* Create(
      AgentSchedulingGroup& agent_scheduling_group,
      const blink::LocalFrameToken& frame_token,
      int32_t routing_id,
      mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
          associated_interface_provider,
      const base::UnguessableToken& devtools_frame_token,
      bool is_for_nested_main_frame);

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  // Checks whether accessibility support for this frame is currently enabled.
  bool IsAccessibilityEnabled() const override;

  // mojom::Frame implementation:
  void CommitSameDocumentNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      CommitSameDocumentNavigationCallback callback) override;
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories) override;
  void SetWantErrorMessageStackTrace() override;
  void Unload(
      bool is_loading,
      blink::mojom::FrameReplicationStatePtr replicated_frame_state,
      const blink::RemoteFrameToken& frame_token,
      blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
      blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces)
      override;
  void Delete(mojom::FrameDeleteIntention intent) override;
  void UndoCommitNavigation(
      bool is_loading,
      blink::mojom::FrameReplicationStatePtr replicated_frame_state,
      const blink::RemoteFrameToken& frame_token,
      blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
      blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces)
      override;
  void GetInterfaceProvider(
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver)
      override;
  void SnapshotAccessibilityTree(
      mojom::SnapshotAccessibilityTreeParamsPtr params,
      SnapshotAccessibilityTreeCallback callback) override;
  void GetSerializedHtmlWithLocalLinks(
      const base::flat_map<GURL, base::FilePath>& url_map,
      const base::flat_map<blink::FrameToken, base::FilePath>& frame_token_map,
      bool save_with_empty_url,
      mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> handler_remote)
      override;

  void OnSerializeMHTMLComplete(
      std::unique_ptr<MHTMLPartsGenerationDelegateImpl> delegate,
      SerializeAsMHTMLCallback callback,
      std::vector<blink::WebThreadSafeData> mhtml_contents,
      blink::WebThreadSafeData frame_mhtml_data);

  // Callback scheduled from SerializeAsMHTML for when writing serialized
  // MHTML to the handle has been completed in the file thread.
  void OnWriteMHTMLComplete(
      SerializeAsMHTMLCallback callback,
      std::unordered_set<std::string> serialized_resources_uri_digests,
      mojom::MhtmlSaveStatus save_status);

  // Requests that the browser process navigates to |url|.
  void OpenURL(std::unique_ptr<blink::WebNavigationInfo> info);

  // Sets `loader_factories_`. And clears `background_resource_fetch_context_`.
  // `background_resource_fetch_context_` will be lazily initialized when
  // creating a WebURLLoader if BackgroundResourceFetch feature is enabled.
  void SetLoaderFactoryBundle(
      scoped_refptr<blink::ChildURLLoaderFactoryBundle> loader_factories);

  scoped_refptr<blink::ChildURLLoaderFactoryBundle> CreateLoaderFactoryBundle(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> info,
      std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          subresource_proxying_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          keep_alive_loader_factory,
      mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
          fetch_later_loader_factory);

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
  // c) function:DidDispatchDOMContentLoadedEvent. When this function is
  // called, that means we have got whole html page. In here we should
  // finally get right encoding of page.
  void UpdateEncoding(blink::WebFrame* frame, const std::string& encoding_name);

  void InitializeMediaStreamDeviceObserver();

  // Called when the RenderFrameImpl is created. This creates and initializes
  // the WebFrameWidget unless this is a LocalFrame<->LocalFrame swap. Widget
  // creation maybe deferred until commit for this case.
  void MaybeInitializeWidget(mojom::CreateFrameWidgetParamsPtr widget_params);

  // Called during a LocalFrame<->LocalFrame swap. This creates and initializes
  // the WebFrameWidget if it was deferred when the RenderFrameImpl was created,
  // see `MaybeInitializeWidget()` above.
  void EnsureWidgetInitialized();

  // Returns the widget whose compositor should be reused for this widget if
  // a non-null `previous_frame_token` is provided.
  blink::WebFrameWidget* PreviousWidgetForLazyCompositorInitialization(
      const std::optional<blink::FrameToken>& previous_frame_token) const;

  // Sends a FrameHostMsg_BeginNavigation to the browser
  void BeginNavigationInternal(std::unique_ptr<blink::WebNavigationInfo> info,
                               bool is_history_navigation_in_new_child_frame,
                               base::TimeTicks renderer_before_unload_start,
                               base::TimeTicks renderer_before_unload_end);

  // TODO(crbug.com/40546539): When creating a new browsing context, Blink
  // always populates it with an initial empty document synchronously, as
  // required by the HTML spec. However, for both iframe and window creation,
  // there is an additional special case that currently requires completing an
  // about:blank navigation synchronously.
  //
  // 1. Inserting an <iframe> into the active document with no src and no
  //    srcdoc or with src = "about:blank".
  // 2. Opening a new window with no specified URL or with URL = "about:blank".
  //
  // In both cases, Blink will initialize the new browsing context, and then
  // immediately re-navigate to "about:blank". This leads to a number of odd
  // situations throughout the navigation stack, and it is spec-incompliant.
  //
  // For a new <iframe>, the re-navigation to "about:blank" should be a regular
  // asynchronous navigation.
  //
  // For a new window, there should be no navigation at all: the standard states
  // that a load event should be dispatched, and nothing else.
  //
  // See also:
  // - https://chromium-review.googlesource.com/c/chromium/src/+/804797
  // - https://github.com/whatwg/html/issues/3267
  void SynchronouslyCommitAboutBlankForBug778318(
      std::unique_ptr<blink::WebNavigationInfo> info);

  // Commit navigation with |navigation_params| prepared.
  void CommitNavigationWithParams(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
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
      mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
      mojo::PendingRemote<blink::mojom::CodeCacheHost>
          code_cache_host_for_background,
      mojom::CookieManagerInfoPtr cookie_manager_info,
      mojom::StorageInfoPtr storage_info,
      std::unique_ptr<DocumentState> document_state,
      std::unique_ptr<blink::WebNavigationParams> navigation_params);

  // Decodes a data url for navigation commit.
  void DecodeDataURL(const blink::mojom::CommonNavigationParams& common_params,
                     const blink::mojom::CommitNavigationParams& commit_params,
                     std::string* mime_type,
                     std::string* charset,
                     std::string* data,
                     GURL* base_url);

  void FinalizeRequestInternal(blink::WebURLRequest& request,
                               bool for_outermost_main_frame,
                               ui::PageTransition transition_type);
  // |transition_type| corresponds to the document which triggered this request.
  std::optional<blink::WebURL> WillSendRequestInternal(
      const blink::WebURL& target,
      const blink::WebSecurityOrigin& security_origin,
      const net::SiteForCookies& site_for_cookies,
      ForRedirect for_redirect,
      const blink::WebURL& upstream_url,
      ui::PageTransition transition_type);

  // Returns the URL being loaded by the |frame_|'s request.
  GURL GetLoadingUrl() const;

  void RegisterMojoInterfaces();

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Send |callback| our AndroidOverlay routing token.
  void RequestOverlayRoutingToken(media::RoutingTokenCallback callback);

  void BindWebUIReceiver(mojo::PendingReceiver<mojom::WebUI> receiver);

  // Build DidCommitProvisionalLoadParams based on the frame internal state.
  mojom::DidCommitProvisionalLoadParamsPtr MakeDidCommitProvisionalLoadParams(
      blink::WebHistoryCommitType commit_type,
      ui::PageTransition transition,
      const blink::ParsedPermissionsPolicy& permissions_policy_header,
      const blink::DocumentPolicyFeatureState& document_policy_header,
      const std::optional<base::UnguessableToken>& embedding_token);

  // Updates the navigation history depending on the passed parameters.
  // This could result either in the creation of a new entry or a modification
  // of the current entry or nothing. If a new entry was created,
  // returns true, false otherwise.
  void UpdateNavigationHistory(blink::WebHistoryCommitType commit_type);

  // Notify render_view_ observers that a commit happened.
  void NotifyObserversOfNavigationCommit(ui::PageTransition transition);

  // Updates the internal state following a navigation commit. This should be
  // called before notifying the FrameHost of the commit.
  void UpdateStateForCommit(blink::WebHistoryCommitType commit_type,
                            ui::PageTransition transition);

  // Internal function used by same document navigation as well as cross
  // document navigation that updates the state of the RenderFrameImpl and sends
  // a commit message to the browser process.
  void DidCommitNavigationInternal(
      blink::WebHistoryCommitType commit_type,
      ui::PageTransition transition,
      const blink::ParsedPermissionsPolicy& permissions_policy_header,
      const blink::DocumentPolicyFeatureState& document_policy_header,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params,
      const std::optional<base::UnguessableToken>& embedding_token);

  blink::WebComputedAXTree* GetOrCreateWebComputedAXTree() override;

  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() override;
  bool IsPluginHandledExternally(
      const blink::WebElement& plugin_element,
      const blink::WebURL& url,
      const blink::WebString& suggested_mime_type) override;
  bool IsDomStorageDisabled() const override;
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate) override;

  // Updates the state of this frame when asked to commit a navigation.
  void PrepareFrameForCommit(
      const GURL& url,
      const blink::mojom::CommitNavigationParams& commit_params);

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
  // TODO(crbug.com/40142288): Support unload-in-commit.
  void SetOldPageLifecycleStateFromNewPageCommitIfNeeded(
      const blink::mojom::OldPageInfo* old_page_info,
      const GURL& new_page_url);

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
      const blink::mojom::CommonNavigationParams& common_params,
      const blink::mojom::CommitNavigationParams& commit_params,
      blink::WebHistoryItem* item_for_history_navigation,
      blink::WebFrameLoadType* load_type);

  // Implements AddMessageToConsole().
  void AddMessageToConsoleImpl(blink::mojom::ConsoleMessageLevel level,
                               const std::string& message,
                               bool discard_duplicates);

  // Start a delayed timer to update the frame sync state to the browser.
  // Debounces many updates in quick succession.
  void StartDelayedSyncTimer();

  // Swaps out the RenderFrame, creating a new `blink::RemoteFrame` and then
  // swapping it into the frame tree to replace `this`. Returns false if
  // swapping out `this` ends up detaching this frame instead when running the
  // unload handlers, and true otherwise.
  //
  // Important: after this method returns, `this` has been deleted.
  bool SwapOutAndDeleteThis(
      bool is_loading,
      blink::mojom::FrameReplicationStatePtr replicated_frame_state,
      const blink::RemoteFrameToken& frame_token,
      blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
      blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces);

  // Resets membmers that are needed for the duration of commit (time between
  // CommitNavigation() and DidCommitNavigation().
  void ResetMembersUsedForDurationOfCommit();

  // Stores the WebLocalFrame we are associated with.  This is null from the
  // constructor until BindToFrame() is called, and it is null after
  // FrameDetached() is called until destruction (which is asynchronous in the
  // case of the main frame, but not subframes).
  raw_ptr<blink::WebNavigationControl> frame_ = nullptr;

  // The `AgentSchedulingGroup` this frame is associated with.
  const raw_ref<AgentSchedulingGroup> agent_scheduling_group_;

  // False until Initialize() is run, to avoid actions before the frame's
  // observers are created.
  bool initialized_ = false;
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
    bool IsCandidateUnique(std::string_view name) const override;
    int GetSiblingCount() const override;
    int GetChildCount() const override;
    std::vector<std::string> CollectAncestorNames(
        BeginPoint begin_point,
        bool (*should_stop)(std::string_view)) const override;
    std::vector<int> GetFramePosition(BeginPoint begin_point) const override;

   private:
    blink::WebLocalFrame* GetWebFrame() const;

    raw_ptr<RenderFrameImpl> render_frame_;
  };
  UniqueNameFrameAdapter unique_name_frame_adapter_;
  blink::UniqueNameHelper unique_name_helper_;

  // Indicates whether the frame has been inserted into the frame tree yet or
  // not.
  //
  // When a frame is created by the browser process, it is for a pending
  // navigation. In this case, it is not immediately attached to the frame tree
  // if there is a `blink::RemoteFrame` for the same frame. It is inserted into
  // the frame tree at the time the pending navigation commits. Frames added by
  // the parent document are created from the renderer process and are
  // immediately inserted in the frame tree.
  // TODO(dcheng): Remove this once we have FrameTreeHandle and can use the
  // Blink Web* layer to check for provisional frames.
  bool in_frame_tree_;

  blink::LocalFrameToken frame_token_;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  const int routing_id_;
#endif

  const int process_label_id_;

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

  // All the registered observers.
  base::ObserverList<RenderFrameObserver>::Unchecked observers_;

  // The text selection the last time DidChangeSelection got called. May contain
  // additional characters before and after the selected text, for IMEs. The
  // portion of this string that is the actual selected text starts at index
  // |selection_range_.GetMin() - selection_text_offset_| and has length
  // |selection_range_.length()|.
  std::u16string selection_text_;
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

  // This interface handles generated code cache requests both to fetch code
  // cache when loading resources and to store code caches when code caches are
  // generated during the JS / Wasm script execution.
  mojo::Remote<blink::mojom::CodeCacheHost> code_cache_host_;

  // The media permission dispatcher attached to this frame.
  std::unique_ptr<MediaPermissionDispatcher> media_permission_dispatcher_;

  service_manager::BinderRegistry registry_;
  std::unique_ptr<BlinkInterfaceRegistryImpl> blink_interface_registry_;

  // If valid, the next ExecutionContext created will enable MojoJS bindings and
  // use this broker to handle Mojo.bindInterface calls.
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      mojo_js_interface_broker_;

  // Valid during the entire life time of the RenderFrame.
  std::unique_ptr<RenderAccessibilityManager> render_accessibility_manager_;

  std::unique_ptr<blink::WeakWrapperResourceLoadInfoNotifier>
      weak_wrapper_resource_load_info_notifier_;

  // Plugins -------------------------------------------------------------------
#if BUILDFLAG(ENABLE_PPAPI)
  typedef std::set<raw_ptr<PepperPluginInstanceImpl, SetExperimental>>
      PepperPluginSet;
  PepperPluginSet active_pepper_instances_;

  // Whether or not the focus is on a PPAPI plugin
  raw_ptr<PepperPluginInstanceImpl> focused_pepper_plugin_;

  mojo::AssociatedRemote<mojom::PepperHost> pepper_host_remote_;
#endif

  using AutoplayOriginAndFlags = std::pair<url::Origin, int32_t>;
  AutoplayOriginAndFlags autoplay_flags_;

  mojo::AssociatedReceiver<blink::mojom::AutoplayConfigurationClient>
      autoplay_configuration_receiver_{this};
  // The mojom::Frame can not be bound at construction because it needs the
  // WebFrame to get a TaskRunner from so it is stashed here.
  mojo::PendingAssociatedReceiver<mojom::Frame> pending_frame_receiver_;
  mojo::AssociatedReceiver<mojom::Frame> frame_receiver_{this};
  mojo::AssociatedReceiver<mojom::FrameBindingsControl>
      frame_bindings_control_receiver_{this};
  mojo::AssociatedReceiver<mojom::MhtmlFileWriter> mhtml_file_writer_receiver_{
      this};

  // There are two different kinds of NavigationClients, the request
  // NavigationClient and the commit NavigationClient.
  //
  // ## Request NavigationClient ##
  // Set if and only if the frame that initiated the navigation and the frame
  // being navigated are both RenderFrameImpls in the same frame tree (i.e. the
  // navigation does not ever go through a RenderFrameProxy). This has
  // interesting implications for behavior differences between the two example
  // frame trees below:
  //
  //   a.com           a.com
  //     ↓               ↓
  //   b.com       example.a.com
  //
  // Assuming the standard site-per-process allocation policy, though (a.com) is
  // cross-origin to both (b.com) and (example.a.com):
  //
  // - (a.com) performing a navigation in (b.com) *will not* create a request
  //   NavigationClient but
  // - (a.com) performing a navigation in (example.a.com) *will* create a
  //   request NavigationClient
  //
  // Note that the initiating RenderFrameImpl does *not* own the request
  // NavigationClient. Rather, the RenderFrameImpl that the navigation *targets*
  // is the RenderFrameImpl that owns the request NavigationClient.
  //
  // ## Commit NavigationClient ##
  // Always set in the RenderFrameImpl that has been selected to commit a
  // navigation. This selection happens when the NavigationRequest in the
  // browser process reaches READY_TO_COMMIT.
  //
  // If a navigation will commit in the same RenderFrameImpl that owns the
  // request NavigationClient, the request NavigationClient will be reused as
  // the commit NavigationClient. The way this works is:
  //   1.) RenderFrameHostImpl::BeginNavigation() accepts an always-bound remote
  //       to the RenderFrameImpl's `navigation_client_impl_`.
  //   2.) In `NavigationRequest::ctor()`, the request consumes the
  //       NavigationClient remote, and
  //       `NavigationRequest::SetNavigationClient()` assigns
  //       `NavigationRequest::request_navigation_client_` to it.
  //   3.) Eventually, `NavigationRequest` picks a `RenderFrameHostImpl` to
  //   commit
  //       to. In `NavigationRequest::CommitNavigation()`, the request needs to
  //       set its `commit_navigation_client_` to the `NavigationClient`
  //       implementation in the target RenderFrameImpl. If we detect that the
  //       navigation will commit to the same frame that
  //       `NavigationRequest::request_navigation_client_` points to, then the
  //       browser will reuse the request navigation client as the commit one.
  //       Otherwise, it requests a *new* client from the renderer, to act as
  //       the target RenderFrameImpl's `NavigationClient`.
  //
  // ## Navigation Cancellation ##
  // Cancellation is signalled by closing the NavigationClient message pipe.
  // This will eventually trigger a connection error in the browser process,
  // which normally invokes NavigationRequest::OnRendererAbortedNavigation().
  //
  // However, once the NavigationRequest reaches READY_TO_COMMIT in the browser
  // process, *only* the commit NavigationClient may cancel the navigation by
  // closing the message pipe. Thus, only navigations which reuse the RFH may be
  // cancelled (or more accurately, ignored) at this point.
  //
  // Unfortunately, this behavior is important for web compatibility: there are
  // sites which depend on a call to window.stop() or document.open() to cancel
  // a same-site navigation: see https://crbug.com/763106 for background.
  //
  // Note that using RenderDocument means that all cross-document navigations
  // will use a provisional RenderFrameImpl: as such, all cross-document
  // navigations with RenderDocument will ignore cancellation after
  // READY_TO_COMMIT. To handle this, all renderer-initiated navigations will
  // not enter the READY_TO_COMMIT stage until the task that initiated the
  // navigation finishes, to ensure that no renderer-initiated navigation
  // cancellation can take place after READY_TO_COMMIT. For more details, see
  // RendererCancellationThrottle.
  std::unique_ptr<NavigationClient> navigation_client_impl_;

  // Creates various media clients.
  MediaFactory media_factory_;

  blink::AssociatedInterfaceRegistry associated_interfaces_;
  // `remote_associated_interfaces_` cannot be constructed/bound at
  // RenderFrameImpl construction because it needs the underlying WebFrame to
  // get a TaskRunner. It also cannot be constructed/bound later in
  // `RenderFrame::Initialize()` (where we bind the `mojom::Frame` receiver),
  // because that happens *after* the `WebFrame` is initialized, and its
  // initialization relies on the interface provider being bound. So we stash
  // the pending remote here, and we bind it lazily in
  // `GetRemoteAssociatedInterfaces()`.
  mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
      pending_associated_interface_provider_remote_;
  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // This flag is true while browser process is processing a pending navigation,
  // as a result of mojom::FrameHost::BeginNavigation call. It is reset when the
  // navigation is either committed or cancelled.
  bool is_requesting_navigation_ = false;

  // Set to true on the first time the RenderFrame started any navigation.
  // Note that when a frame is created it will trigger a navigation (either
  // synchronous to an empty document or asynchronous through the browser
  // process), so this will only stay false until we triggered that navigation.
  bool had_started_any_navigation_ = false;

  // The bindings types that have been enabled for this RenderFrame.
  BindingsPolicySet enabled_bindings_;

  // This boolean indicates whether JS bindings for Mojo should be enabled at
  // the time the next script context is created.
  bool enable_mojo_js_bindings_ = false;

  // This struct describes a set of MojoJs features to be enabled when the next
  // script context is created (requires MojoJs to be enabled).
  content::mojom::ExtraMojoJsFeaturesPtr mojo_js_features_;

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
  scoped_refptr<blink::ChildURLLoaderFactoryBundle> loader_factories_;

  // Loader factory bundle is stored here temporary between CommitNavigation
  // and DidCommitNavigation calls. These happen synchronously one after
  // another.
  scoped_refptr<blink::ChildURLLoaderFactoryBundle> pending_loader_factories_;

  // The context used for background resource fetch. Used only when
  // BackgroundResourceFetch feature is enabled.
  scoped_refptr<blink::WebBackgroundResourceFetchAssets>
      background_resource_fetch_context_;
  // Used for background resource fetch.
  scoped_refptr<base::SequencedTaskRunner>
      background_resource_fetch_task_runner_;

  mojo::PendingRemote<blink::mojom::CodeCacheHost> pending_code_cache_host_;
  mojo::PendingRemote<blink::mojom::CodeCacheHost>
      pending_code_cache_host_for_background_;
  mojom::CookieManagerInfoPtr pending_cookie_manager_info_;
  mojom::StorageInfoPtr pending_storage_info_;
  // The storage key which |pending_storage_info_| is associated with.
  blink::StorageKey original_storage_key_;

  // AndroidOverlay routing token from the browser, if we have one yet.
  std::optional<base::UnguessableToken> overlay_routing_token_;

  // Used for devtools instrumentation and trace-ability. This token is
  // used to tag calls and requests in order to attribute them to the context
  // frame.
  // |devtools_frame_token_| is only defined by the browser and is never
  // sent back from the renderer in the control calls.
  base::UnguessableToken devtools_frame_token_;

  // True if the frame host wants stack traces on JavaScript console messages of
  // kError severity.
  bool want_error_message_stack_trace_ = false;

  // Contains a representation of the accessibility tree stored in content for
  // use inside of Blink.
  std::unique_ptr<blink::WebComputedAXTree> computed_ax_tree_;

  // Used for tracking a frame's main frame document intersection and
  // replicating it to the browser when it changes.
  std::optional<gfx::Rect> main_frame_intersection_rect_;

  // Used for tracking the main frame viewport rectangle (i.e. dimensions and
  // scroll offset) within the main frame document.
  std::optional<gfx::Rect> main_frame_viewport_rect_;

  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  RenderFrameMediaPlaybackOptions renderer_media_playback_options_;

  class MHTMLBodyLoaderClient;
  std::unique_ptr<MHTMLBodyLoaderClient> mhtml_body_loader_client_;

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_override_for_test_;

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

  // Timer used to delay the updating of frame state.
  base::OneShotTimer delayed_state_sync_timer_;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  bool send_content_state_immediately_ = false;

  // The RenderFrameImpl can be created in 2 modes.
  //
  // 1. The associated WebFrameWidget and its compositor is initialized at
  //    creation time. This is default mode.
  //
  // 2. The associated WebFrameWidget and its compositor is initialized at
  //    commit time. This is done for local RF->local RF navigations to reuse
  //    the compositor from the previous RFH. This is purely a performance
  //    optimization.
  //
  // When in mode 2, the parameters to create the WebFrameWidget (which are
  // part of the IPC that created this frame) are cached until commit to lazily
  // create the WebFrameWidget.
  mojom::CreateFrameWidgetParamsPtr widget_params_for_lazy_widget_creation_;

  // Set when this RenderFrame is being swapped for
  // `provisional_frame_for_local_root_swap_`.
  base::WeakPtr<RenderFrameImpl> provisional_frame_for_local_root_swap_ =
      nullptr;

  // Set if this RenderFrameImpl is for a main frame which is not top-level.
  const bool is_for_nested_main_frame_;

  base::WeakPtrFactory<RenderFrameImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
