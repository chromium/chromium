// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_IMPL_H_

#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/mem/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/inspect/cpp/vmo/types.h>
#include <lib/zx/channel.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/scoped_fx_logger.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/timer/timer.h"
#include "build/chromecast_buildflags.h"
#include "components/media_control/browser/media_blocker.h"
#include "components/on_load_script_injector/browser/on_load_script_injector_host.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia_web/webengine/browser/event_filter.h"
#include "fuchsia_web/webengine/browser/frame_permission_controller.h"
#include "fuchsia_web/webengine/browser/navigation_controller_impl.h"
#include "fuchsia_web/webengine/browser/theme_manager.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_impl.h"
#include "ui/aura/window_tree_host.h"
#include "ui/platform_window/fuchsia/view_ref_pair.h"
#include "ui/wm/core/focus_controller.h"
#include "url/gurl.h"

namespace content {
class FromRenderFrameHost;
class ScopedAccessibilityMode;
}  // namespace content

class ContextImpl;
class FrameWindowTreeHost;
class FrameLayoutManager;
class MediaPlayerImpl;
class NavigationPolicyHandler;

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
class ReceiverSessionClient;
#endif

// Implementation of fuchsia.web.Frame based on content::WebContents.
class WEB_ENGINE_EXPORT FrameImpl : public fuchsia::web::Frame,
                                    public content::WebContentsObserver,
                                    public content::WebContentsDelegate {
 public:
  // Returns FrameImpl that owns the |web_contents| or nullptr if the
  // |web_contents| is nullptr. Returns nullptr if there is no FrameImpl that
  // owns the |web_contents|, which can happen if FrameImpl has not been
  // initialized yet.
  static FrameImpl* FromWebContents(content::WebContents* web_contents);

  // Returns FrameImpl that owns the |render_frame_host| or nullptr if the
  // |render_frame_host| is nullptr. Returns nullptr if there is no FrameImpl
  // that owns the |web_contents|, which can happen if FrameImpl has not been
  // initialized yet.
  static FrameImpl* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  // |context| must out-live |this|.
  // |params| apply both to this Frame, and also to any popup Frames it creates
  // and must be clonable.
  // TODO(fxbug.dev/65750): Consider removing this restriction if clients become
  // responsible for providing parameters for [each] popup and (cloning)
  // |params_for_popups_| is no longer necessary.

  // |inspect_node| will be populated with diagnostic data for this Frame.
  // DestroyFrame() is automatically called on |context| if the |frame_request|
  // channel disconnects.
  FrameImpl(std::unique_ptr<content::WebContents> web_contents,
            ContextImpl* context,
            fuchsia::web::CreateFrameParams params,
            inspect::Node inspect_node,
            fidl::InterfaceRequest<fuchsia::web::Frame> frame_request);
  ~FrameImpl() override;

  FrameImpl(const FrameImpl&) = delete;
  FrameImpl& operator=(const FrameImpl&) = delete;

  const fuchsia::web::FrameMediaSettings& media_settings() const {
    return media_settings_;
  }

  FramePermissionController* permission_controller() {
    return &permission_controller_;
  }

  url_rewrite::UrlRequestRewriteRulesManager*
  url_request_rewrite_rules_manager() {
    return &url_request_rewrite_rules_manager_;
  }

  NavigationPolicyHandler* navigation_policy_handler() {
    return navigation_policy_handler_.get();
  }

  // Enables explicit sites filtering and set the error page. If |error_page| is
  // empty, the default error page will be used.
  void EnableExplicitSitesFilter(std::string error_page);

  const std::optional<std::string>& explicit_sites_filter_error_page() const {
    return explicit_sites_filter_error_page_;
  }

  // Override |blink_prefs| with settings defined in |content_settings_|.
  //
  // This method is called when WebPreferences is first created and when it is
  // recomputed.
  void OverrideWebPreferences(blink::web_pref::WebPreferences* web_prefs);

  // Accessors required by tests.
  zx::unowned_channel GetBindingChannelForTest() const;
  content::WebContents* web_contents_for_test() const {
    return web_contents_.get();
  }
  bool has_view_for_test() const { return window_tree_host_ != nullptr; }
  FrameWindowTreeHost* window_tree_host_for_test() {
    return window_tree_host_.get();
  }

  void set_window_size_for_test(gfx::Size size) {
    window_size_for_test_ = size;
  }

  void set_device_scale_factor_for_test(float device_scale_factor) {
    device_scale_factor_for_test_ = device_scale_factor;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, DelayedNavigationEventAck);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NavigationObserverDisconnected);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NoNavigationObserverAttached);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, ReloadFrame);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, Stop);
  FRIEND_TEST_ALL_PREFIXES(FuchsiaFrameAccessibilityTest, HitTest);

  // Used for storing awaiting popup frames in |pending_popups_|
  struct PendingPopup {
    PendingPopup(FrameImpl* frame_ptr,
                 fidl::InterfaceHandle<fuchsia::web::Frame> handle,
                 fuchsia::web::PopupFrameCreationInfo creation_info);
    PendingPopup(PendingPopup&& other);
    ~PendingPopup();

    FrameImpl* frame_ptr;
    fidl::InterfaceHandle<fuchsia::web::Frame> handle;
    fuchsia::web::PopupFrameCreationInfo creation_info;
  };

  aura::Window* root_window() const;

  // Shared implementation for the ExecuteJavaScript[NoResult]() APIs.
  void ExecuteJavaScriptInternal(std::vector<std::string> origins,
                                 fuchsia::mem::Buffer script,
                                 ExecuteJavaScriptCallback callback,
                                 bool need_result);

  // Sends the next entry in |pending_popups_| to |popup_listener_|.
  void MaybeSendPopup();

  void OnPopupListenerDisconnected(zx_status_t status);

  // Cleans up the MediaPlayerImpl on disconnect.
  void OnMediaPlayerDisconnect();

  // An error handler for |accessibility_bridge_|.
  bool OnAccessibilityError(zx_status_t error);

  // Creates and initializes WindowTreeHost for the view with the specified
  // |view_token|. |view_token| may be uninitialized in headless mode.
  void SetupWindowTreeHost(fuchsia::ui::views::ViewToken view_token,
                           ui::ViewRefPair view_ref_pair);

  // Creates and initializes WindowTreeHost for the view with the specified
  // |view_creation_token|. |view_creation_token| may be uninitialized in
  // headless mode.
  void SetupWindowTreeHost(
      fuchsia::ui::views::ViewCreationToken view_creation_token,
      ui::ViewRefPair view_ref_pair);

  // Initializes WindowTreeHost.
  void InitWindowTreeHost();

  // Destroys the WindowTreeHost along with its view or other associated
  // resources.
  void DestroyWindowTreeHost();

  // Destroys |this| and sends the FIDL |error| to the client.
  void CloseAndDestroyFrame(zx_status_t error);

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  // Determines whether |message| is a Cast Streaming message and if so, handles
  // it. Returns whether it handled the message, regardless of whether that was
  // successful. If true is returned, |callback| has been called. Returns false
  // immediately if Cast Streaming support is not enabled. Called by
  // PostMessage().
  bool MaybeHandleCastStreamingMessage(std::string* origin,
                                       fuchsia::web::WebMessage* message,
                                       PostMessageCallback* callback);

  void MaybeStartCastStreaming(content::NavigationHandle* navigation_handle);
#endif

  // Updates zoom level for the specified |render_frame_host|.
  void UpdateRenderFrameZoomLevel(content::RenderFrameHost* render_frame_host);

  // Helper method for connecting to AccessibilityBridge on
  // |accessibility_bridge_|.
  void ConnectToAccessibilityBridge();

  // Shared implementation of CreateView and CreateViewWithViewRef.
  void CreateViewImpl(fuchsia::ui::views::ViewToken view_token,
                      fuchsia::ui::views::ViewRefControl control_ref,
                      fuchsia::ui::views::ViewRef view_ref);

  // fuchsia::web::Frame implementation.
  void CreateView(fuchsia::ui::views::ViewToken view_token) override;
  void CreateViewWithViewRef(fuchsia::ui::views::ViewToken view_token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override;
  void CreateView2(fuchsia::web::CreateView2Args view_args) override;
  void GetMediaPlayer(fidl::InterfaceRequest<fuchsia::media::sessions2::Player>
                          player) override;
  void GetNavigationController(
      fidl::InterfaceRequest<fuchsia::web::NavigationController> controller)
      override;
  void ExecuteJavaScript(std::vector<std::string> origins,
                         fuchsia::mem::Buffer script,
                         ExecuteJavaScriptCallback callback) override;
  void ExecuteJavaScriptNoResult(
      std::vector<std::string> origins,
      fuchsia::mem::Buffer script,
      ExecuteJavaScriptNoResultCallback callback) override;
  void AddBeforeLoadJavaScript(
      uint64_t id,
      std::vector<std::string> origins,
      fuchsia::mem::Buffer script,
      AddBeforeLoadJavaScriptCallback callback) override;
  void RemoveBeforeLoadJavaScript(uint64_t id) override;
  void PostMessage(std::string origin,
                   fuchsia::web::WebMessage message,
                   fuchsia::web::Frame::PostMessageCallback callback) override;
  void SetNavigationEventListener(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener)
      override;
  void SetNavigationEventListener2(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener,
      fuchsia::web::NavigationEventListenerFlags flags) override;
  void SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel level) override;
  void SetConsoleLogSink(fuchsia::logger::LogSinkHandle sink) override;
  void ConfigureInputTypes(fuchsia::web::InputTypes types,
                           fuchsia::web::AllowInputState allow) override;
  void SetPopupFrameCreationListener(
      fidl::InterfaceHandle<fuchsia::web::PopupFrameCreationListener> listener)
      override;
  void SetUrlRequestRewriteRules(
      std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
      SetUrlRequestRewriteRulesCallback callback) override;
  void EnableHeadlessRendering() override;
  void DisableHeadlessRendering() override;
  void SetMediaSettings(
      fuchsia::web::FrameMediaSettings media_settings) override;
  void ForceContentDimensions(
      std::unique_ptr<fuchsia::ui::gfx::vec2> web_dips) override;
  void SetPermissionState(fuchsia::web::PermissionDescriptor permission,
                          std::string web_origin,
                          fuchsia::web::PermissionState state) override;
  void SetBlockMediaLoading(bool blocked) override;
  void GetPrivateMemorySize(GetPrivateMemorySizeCallback callback) override;
  void SetNavigationPolicyProvider(
      fuchsia::web::NavigationPolicyProviderParams params,
      fidl::InterfaceHandle<fuchsia::web::NavigationPolicyProvider> provider)
      override;
  void SetContentAreaSettings(
      fuchsia::web::ContentAreaSettings settings) override;
  void ResetContentAreaSettings() override;
  void Close(fuchsia::web::FrameCloseRequest request) override;

  // content::WebContentsDelegate implementation.
  void CloseContents(content::WebContents* source) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  std::unique_ptr<content::AudioStreamBrokerFactory>
  CreateAudioStreamBrokerFactory(content::WebContents* web_contents) override;
  bool CanOverscrollContent() override;

  // content::WebContentsObserver implementation.
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void RenderFrameCreated(content::RenderFrameHost* frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;

  // Notified whenever the pixel scale of the `Frame`'s `View` changes.
  void OnPixelScaleUpdate(float pixel_scale);

  // Called by the `accessibility_bridge_` in response to changes in the
  // system's "semantics mode" setting.
  void SetAccessibilityEnabled(bool enabled);

  // Called by `theme_manager_` if it is unable to determine the system theme.
  void OnThemeManagerError();

  const std::unique_ptr<content::WebContents> web_contents_;
  ContextImpl* const context_;

  // Optional tag to apply when emitting web console logs.
  const std::string console_log_tag_;

  // Logger used for console messages from content, depending on |log_level_|.
  base::ScopedFxLogger console_logger_;
  logging::LogSeverity log_level_ = logging::LOGGING_NUM_SEVERITIES;

  // Parameters applied to popups created by content running in this Frame.
  const fuchsia::web::CreateFrameParams params_for_popups_;

  base::RepeatingCallback<void(fuchsia::media::AudioRenderUsage output_usage)>
      set_audio_output_usage_callback_;

  std::unique_ptr<FrameWindowTreeHost> window_tree_host_;

  std::unique_ptr<wm::FocusController> focus_controller_;

  // Owned via |window_tree_host_|.
  FrameLayoutManager* layout_manager_ = nullptr;

  std::unique_ptr<ui::AccessibilityBridgeFuchsiaImpl> accessibility_bridge_;

  // Test settings.
  std::optional<gfx::Size> window_size_for_test_;
  std::optional<float> device_scale_factor_for_test_;

  EventFilter event_filter_;
  NavigationControllerImpl navigation_controller_;
  url_rewrite::UrlRequestRewriteRulesManager url_request_rewrite_rules_manager_;
  FramePermissionController permission_controller_;
  std::unique_ptr<NavigationPolicyHandler> navigation_policy_handler_;

  fuchsia::web::FrameMediaSettings media_settings_;

  // Stored settings for web contents in the current Frame.
  fuchsia::web::ContentAreaSettings content_area_settings_;

  // Used for receiving and dispatching popup created by this Frame.
  fuchsia::web::PopupFrameCreationListenerPtr popup_listener_;
  std::list<PendingPopup> pending_popups_;
  bool popup_ack_outstanding_ = false;
  gfx::Size render_size_override_;

  std::unique_ptr<MediaPlayerImpl> media_player_;
  on_load_script_injector::OnLoadScriptInjectorHost<uint64_t> script_injector_;

  fidl::Binding<fuchsia::web::Frame> binding_;
  media_control::MediaBlocker media_blocker_;

  ThemeManager theme_manager_;

  // The error page to be displayed when a navigation to an explicit site is
  // filtered. Explicit sites are filtered if it has a value. If set to the
  // empty string, the default error page will be displayed.
  std::optional<std::string> explicit_sites_filter_error_page_;

  // Used to publish Frame details to Inspect.
  inspect::Node inspect_node_;
  const inspect::StringProperty inspect_name_property_;

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  std::unique_ptr<ReceiverSessionClient> receiver_session_client_;
#endif

  // Used to implement graceful `Close()` with `timeout` specified.
  base::OneShotTimer close_page_timeout_;

  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

  base::WeakPtrFactory<FrameImpl> weak_factory_{this};
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_IMPL_H_
