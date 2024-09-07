// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/guest_view/browser/guest_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/permission_result.h"
#include "extensions/browser/guest_view/web_view/javascript_dialog_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_find_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "extensions/browser/script_executor.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

namespace content {
class StoragePartitionConfig;
}

namespace extensions {

class WebViewInternalFindFunction;

// A WebViewGuest provides the browser-side implementation of the <webview> API
// and manages the dispatch of <webview> extension events. WebViewGuest is
// created on attachment. That is, when a guest WebContents is associated with
// a particular embedder WebContents. This happens on either initial navigation
// or through the use of the New Window API, when a new window is attached to
// a particular <webview>.
class WebViewGuest : public guest_view::GuestView<WebViewGuest> {
 public:
  ~WebViewGuest() override;
  WebViewGuest(const WebViewGuest&) = delete;
  WebViewGuest& operator=(const WebViewGuest&) = delete;

  static std::unique_ptr<GuestViewBase> Create(
      content::RenderFrameHost* owner_rfh);
  // Cleans up state when this GuestView is being destroyed.
  // Note that this cannot be done in the destructor since a GuestView could
  // potentially be created and destroyed in JavaScript before getting a
  // GuestViewBase instance.
  static void CleanUp(content::BrowserContext* browser_context,
                      int embedder_process_id,
                      int view_instance_id);

  static const char Type[];
  static const guest_view::GuestViewHistogramValue HistogramValue;

  // Returns the WebView partition ID associated with the render process
  // represented by |render_process_host|, if any. Otherwise, an empty string is
  // returned.
  static std::string GetPartitionID(
      content::RenderProcessHost* render_process_host);

  // Returns the stored rules registry ID of the given webview. Will generate
  // an ID for the first query.
  static int GetOrGenerateRulesRegistryID(
      int embedder_process_id,
      int web_view_instance_id);

  // Get the current zoom.
  double GetZoom() const;

  // Get the current zoom mode.
  zoom::ZoomController::ZoomMode GetZoomMode();

  // Request navigating the guest to the provided |src| URL.
  void NavigateGuest(const std::string& src,
                     base::OnceCallback<void(content::NavigationHandle&)>
                         navigation_handle_callback,
                     bool force_navigation);

  // Shows the context menu for the guest.
  void ShowContextMenu(int request_id);

  int rules_registry_id() const { return rules_registry_id_; }

  // Sets the frame name of the guest.
  void SetName(const std::string& name);
  const std::string& name() { return name_; }

  // Set the zoom factor.
  void SetZoom(double zoom_factor);

  // Set the zoom mode.
  void SetZoomMode(zoom::ZoomController::ZoomMode zoom_mode);

  void SetAllowScaling(bool allow);
  bool allow_scaling() const { return allow_scaling_; }

  // Sets the transparency of the guest.
  void SetAllowTransparency(bool allow);
  bool allow_transparency() const { return allow_transparency_; }

  // Sets the audio muted state of the guest.
  void SetAudioMuted(bool mute);
  bool IsAudioMuted();

  // Begin or continue a find request.
  void StartFind(const std::u16string& search_text,
                 blink::mojom::FindOptionsPtr options,
                 scoped_refptr<WebViewInternalFindFunction> find_function);

  // Conclude a find request to clear highlighting.
  void StopFinding(content::StopFindAction);

  // If possible, navigate the guest to |relative_index| entries away from the
  // current navigation entry. Returns true on success.
  bool Go(int relative_index);

  // Reload the guest.
  void Reload();

  // Overrides the user agent for this guest.
  // This affects subsequent guest navigations.
  void SetUserAgentOverride(const std::string& user_agent_override);

  // Stop loading the guest.
  void Stop();

  // Kill the guest process.
  void Terminate();

  // Clears data in the storage partition of this guest.
  //
  // Partition data that are newer than |removal_since| will be removed.
  // |removal_mask| corresponds to bitmask in StoragePartition::RemoveDataMask.
  bool ClearData(const base::Time remove_since,
                 uint32_t removal_mask,
                 base::OnceClosure callback);

  ScriptExecutor* script_executor() { return script_executor_.get(); }
  WebViewPermissionHelper* web_view_permission_helper() {
    return web_view_permission_helper_.get();
  }

  // Enables or disables spatial navigation.
  void SetSpatialNavigationEnabled(bool enabled);

  // Returns spatial navigation status.
  bool IsSpatialNavigationEnabled() const;

  base::WeakPtr<WebViewGuest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  explicit WebViewGuest(content::RenderFrameHost* owner_rfh);

  void ClearCodeCache(base::Time remove_since,
                      uint32_t removal_mask,
                      base::OnceClosure callback);
  void ClearDataInternal(const base::Time remove_since,
                         uint32_t removal_mask,
                         base::OnceClosure callback);

  void OnWebViewNewWindowResponse(int new_window_instance_id,
                                  bool allow,
                                  const std::string& user_input);

  void OnFullscreenPermissionDecided(bool allowed,
                                     const std::string& user_input);
  bool GuestMadeEmbedderFullscreen() const;
  void SetFullscreenState(bool is_fullscreen);

  void RequestPointerLockPermission(bool user_gesture,
                                    bool last_unlocked_by_target,
                                    base::OnceCallback<void(bool)> callback);

  // GuestViewBase implementation.
  void CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                         const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) final;
  void DidAttachToEmbedder() final;
  void DidInitialize(const base::Value::Dict& create_params) final;
  void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) final;
  void EmbedderFullscreenToggled(bool entered_fullscreen) final;
  void FindReply(content::WebContents* source,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) final;
  bool ZoomPropagatesFromEmbedderToGuest() const final;
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;
  void GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                     const gfx::Size& new_size) final;
  void GuestViewDidStopLoading() final;
  void GuestZoomChanged(double old_zoom_level, double new_zoom_level) final;
  bool IsAutoSizeSupported() const final;
  void OnOwnerAudioMutedStateUpdated(bool muted) final;
  void SignalWhenReady(base::OnceClosure callback) final;
  void WillAttachToEmbedder() final;
  bool RequiresSslInterstitials() const final;
  bool IsPermissionRequestable(ContentSettingsType type) const final;
  std::optional<content::PermissionResult> OverridePermissionResult(
      ContentSettingsType type) const final;

  // WebContentsDelegate implementation.
  void CloseContents(content::WebContents* source) final;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) final;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) final;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) final;
  void RendererResponsive(content::WebContents* source,
                          content::RenderWidgetHost* render_widget_host) final;
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) final;
  void RequestMediaAccessPermission(
      content::WebContents* source,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) final;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) final;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) final;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) final;
  bool ShouldResumeRequestsForCreatedWindow() override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) final;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) final;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) final;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) final;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) final;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) final;
  void RequestPointerLock(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;

  // WebContentsObserver implementation.
  void DidStartNavigation(content::NavigationHandle* navigation_handle) final;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) final;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;
  void LoadProgressChanged(double progress) final;
  void DocumentOnLoadCompletedInPrimaryMainFrame() final;
  void PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) final;
  void UserAgentOverrideSet(const blink::UserAgentOverride& ua_override) final;
  void FrameNameChanged(content::RenderFrameHost* render_frame_host,
                        const std::string& name) final;
  void OnAudioStateChanged(bool audible) final;
  void OnDidAddMessageToConsole(
      content::RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) final;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) final;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) final;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) final;
  void WebContentsDestroyed() final;

  // Informs the embedder of a frame name change.
  void ReportFrameNameChange(const std::string& name);

  void PushWebViewStateToIOThread(content::RenderFrameHost* guest_host);

  // Loads the `url` provided. `force_navigation` indicates whether to reload
  // the content if the provided `url` matches the current page of the guest.
  // If a `navigation_handle_callback` function is provided, it should be called
  // with the pending navigation (if any) when the navigation handle become
  // available. This allows callers to observe or attach their specific data.
  // This function may not be called if the navigation fails for any reason.
  void LoadURLWithParams(const GURL& url,
                         const content::Referrer& referrer,
                         ui::PageTransition transition_type,
                         base::OnceCallback<void(content::NavigationHandle&)>
                             navigation_handle_callback,
                         bool force_navigation);

  void RequestNewWindowPermission(WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_bounds,
                                  std::unique_ptr<WebViewGuest> new_guest);

  // Requests resolution of a potentially relative URL.
  GURL ResolveURL(const std::string& src);

  // Notification that a load in the guest resulted in abort. Note that |url|
  // may be invalid.
  void LoadAbort(bool is_top_level, const GURL& url, int error_code);

  // Creates a new guest window owned by this WebViewGuest.
  void CreateNewGuestWebViewWindow(const content::OpenURLParams& params);

  void NewGuestWebViewCallback(const content::OpenURLParams& params,
                               std::unique_ptr<GuestViewBase> guest);

  bool HandleKeyboardShortcuts(const input::NativeWebKeyboardEvent& event);

  void ApplyAttributes(const base::Value::Dict& params);

  void SetTransparency(content::RenderFrameHost* render_frame_host);

  void CreateWebContentsWithStoragePartition(
      std::unique_ptr<GuestViewBase> owned_this,
      const base::Value::Dict& create_params,
      WebContentsCreatedCallback callback,
      std::optional<content::StoragePartitionConfig> storage_partition_config);

  // Identifies the set of rules registries belonging to this guest.
  int rules_registry_id_;

  // Handles find requests and replies for the webview find API.
  WebViewFindHelper find_helper_;
  std::unique_ptr<ScriptExecutor> script_executor_;

  // True if the user agent is overridden.
  bool is_overriding_user_agent_ = false;

  // Stores the window name of the main frame of the guest.
  std::string name_;

  // Stores whether the contents of the guest can be transparent.
  bool allow_transparency_ = false;

  // Stores whether the guest has been muted by the webview.setAudioMuted API.
  bool is_audio_muted_ = false;

  // Handles the JavaScript dialog requests.
  JavaScriptDialogHelper javascript_dialog_helper_;

  // Handles permission requests.
  std::unique_ptr<WebViewPermissionHelper> web_view_permission_helper_;

  std::unique_ptr<WebViewGuestDelegate> web_view_guest_delegate_;

  // Tracks the name, and target URL of the new window. Once the first
  // navigation commits, we no longer track this information.
  struct NewWindowInfo {
    // Name of the new window.
    std::string name;

    // Expected initial URL of the new window.
    GURL url;

    // Whether OpenURL navigation from the newly created GuestView has changed
    // |url|. The pending OpenURL navigation needs to be applied after attaching
    // the GuestView.
    bool url_changed_via_open_url = false;

    // Whether the newly created GuestView begun navigating away from the
    // initial URL.  Used to suppress the initial navigation when attaching the
    // GuestView and applying its attributes.
    bool did_start_navigating_away_from_initial_url = false;

    NewWindowInfo(const GURL& url, const std::string& name);
    NewWindowInfo(const NewWindowInfo&);
    ~NewWindowInfo();
  };

  using PendingWindowMap = std::map<WebViewGuest*, NewWindowInfo>;
  PendingWindowMap pending_new_windows_;

  // Determines if this guest accepts pinch-zoom gestures.
  bool allow_scaling_ = false;
  bool is_guest_fullscreen_ = false;
  bool is_embedder_fullscreen_ = false;
  bool last_fullscreen_permission_was_allowed_by_embedder_ = false;

  // Tracks whether the webview has a pending zoom from before the first
  // navigation. This will be equal to 0 when there is no pending zoom.
  double pending_zoom_factor_ = 0.0;

  // Whether the GuestView set an explicit zoom level.
  bool did_set_explicit_zoom_ = false;

  // Store spatial navigation status.
  bool is_spatial_navigation_enabled_;

  // Used to delay the navigation of a recreated guest contents until later in
  // the attachment process when state related to the WebRequest API is set up.
  base::OnceClosure recreate_initial_nav_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<WebViewGuest> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
