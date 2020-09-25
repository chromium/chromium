// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/guest_view/browser/guest_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "extensions/browser/guest_view/web_view/javascript_dialog_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_find_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "extensions/browser/script_executor.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

namespace content {
class StoragePartitionConfig;
}  // namespace content

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
  // Clean up state when this GuestView is being destroyed. See
  // GuestViewBase::CleanUp().
  static void CleanUp(content::BrowserContext* browser_context,
                      int embedder_process_id,
                      int view_instance_id);

  static GuestViewBase* Create(content::WebContents* owner_web_contents);

  // For WebViewGuest, we create special guest processes, which host the
  // tag content separately from the main application that embeds the tag.
  // A <webview> can specify both the partition name and whether the storage
  // for that partition should be persisted. Each tag gets a SiteInstance with
  // a specially formatted URL, based on the application it is hosted by and
  // the partition requested by it. The format for that URL is:
  // chrome-guest://partition_domain/persist?partition_name
  static bool GetGuestPartitionConfigForSite(
      const GURL& site,
      content::StoragePartitionConfig* storage_partition_config);

  // Opposite of GetGuestPartitionConfigForSite: Creates a specially formatted
  // URL used by the SiteInstance associated with the WebViewGuest. See
  // GetGuestPartitionConfigForSite for the URL format.
  static GURL GetSiteForGuestPartitionConfig(
      const content::StoragePartitionConfig& storage_partition_config);

  // Returns the WebView partition ID associated with the render process
  // represented by |render_process_host|, if any. Otherwise, an empty string is
  // returned.
  static std::string GetPartitionID(
      content::RenderProcessHost* render_process_host);

  static const char Type[];

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
  void NavigateGuest(const std::string& src, bool force_navigation);

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

  // Loads a data URL with a specified base URL and virtual URL.
  bool LoadDataWithBaseURL(const std::string& data_url,
                           const std::string& base_url,
                           const std::string& virtual_url,
                           std::string* error);

  // Begin or continue a find request.
  void StartFind(const base::string16& search_text,
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
                 const base::Closure& callback);

  ScriptExecutor* script_executor() { return script_executor_.get(); }

  // Enables or disables spatial navigation.
  void SetSpatialNavigationEnabled(bool enabled);

  // Returns spatial navigation status.
  bool IsSpatialNavigationEnabled() const;

 private:
  friend class WebViewPermissionHelper;

  explicit WebViewGuest(content::WebContents* owner_web_contents);

  ~WebViewGuest() override;

  void ClearCodeCache(base::Time remove_since,
                      uint32_t removal_mask,
                      const base::Closure& callback);
  void ClearDataInternal(const base::Time remove_since,
                         uint32_t removal_mask,
                         const base::Closure& callback);

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

  // TODO(533069): This appears to be dead code following BrowserPlugin removal.
  // Investigate removing this.
  void DidDropLink(const GURL& url);

  // GuestViewBase implementation.
  void CreateWebContents(const base::DictionaryValue& create_params,
                         WebContentsCreatedCallback callback) final;
  void DidAttachToEmbedder() final;
  void DidInitialize(const base::DictionaryValue& create_params) final;
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
  void GuestDestroyed() final;
  void GuestReady() final;
  void GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                     const gfx::Size& new_size) final;
  void GuestViewDidStopLoading() final;
  void GuestZoomChanged(double old_zoom_level, double new_zoom_level) final;
  bool IsAutoSizeSupported() const final;
  void SignalWhenReady(base::OnceClosure callback) final;
  void WillAttachToEmbedder() final;
  void WillDestroy() final;

  // WebContentsDelegate implementation.
  void CloseContents(content::WebContents* source) final;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) final;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const content::NativeWebKeyboardEvent& event) final;
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
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) final;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) final;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) final;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) final;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) final;
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
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;

  // WebContentsObserver implementation.
  void DidStartNavigation(content::NavigationHandle* navigation_handle) final;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) final;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) final;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;
  void LoadProgressChanged(double progress) final;
  void DocumentOnLoadCompletedInMainFrame() final;
  void RenderProcessGone(base::TerminationStatus status) final;
  void UserAgentOverrideSet(const blink::UserAgentOverride& ua_override) final;
  void FrameNameChanged(content::RenderFrameHost* render_frame_host,
                        const std::string& name) final;
  void OnAudioStateChanged(bool audible) final;
  void OnDidAddMessageToConsole(blink::mojom::ConsoleMessageLevel log_level,
                                const base::string16& message,
                                int32_t line_no,
                                const base::string16& source_id) final;

  // Informs the embedder of a frame name change.
  void ReportFrameNameChange(const std::string& name);

  void PushWebViewStateToIOThread();

  // Loads the |url| provided. |force_navigation| indicates whether to reload
  // the content if the provided |url| matches the current page of the guest.
  void LoadURLWithParams(
      const GURL& url,
      const content::Referrer& referrer,
      ui::PageTransition transition_type,
      bool force_navigation);

  void RequestNewWindowPermission(
      WindowOpenDisposition disposition,
      const gfx::Rect& initial_bounds,
      content::WebContents* new_contents);

  // Requests resolution of a potentially relative URL.
  GURL ResolveURL(const std::string& src);

  // Notification that a load in the guest resulted in abort. Note that |url|
  // may be invalid.
  void LoadAbort(bool is_top_level, const GURL& url, int error_code);

  // Creates a new guest window owned by this WebViewGuest.
  void CreateNewGuestWebViewWindow(const content::OpenURLParams& params);

  void NewGuestWebViewCallback(const content::OpenURLParams& params,
                               content::WebContents* guest_web_contents);

  bool HandleKeyboardShortcuts(const content::NativeWebKeyboardEvent& event);

  void ApplyAttributes(const base::DictionaryValue& params);

  void SetTransparency();

  // Identifies the set of rules registries belonging to this guest.
  int rules_registry_id_;

  // Handles find requests and replies for the webview find API.
  WebViewFindHelper find_helper_;
  std::unique_ptr<ScriptExecutor> script_executor_;

  // True if the user agent is overridden.
  bool is_overriding_user_agent_;

  // Stores the window name of the main frame of the guest.
  std::string name_;

  // Stores whether the contents of the guest can be transparent.
  bool allow_transparency_;

  // Stores the src URL of the WebView.
  GURL src_;

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
  bool allow_scaling_;
  bool is_guest_fullscreen_;
  bool is_embedder_fullscreen_;
  bool last_fullscreen_permission_was_allowed_by_embedder_;

  // Tracks whether the webview has a pending zoom from before the first
  // navigation. This will be equal to 0 when there is no pending zoom.
  double pending_zoom_factor_;

  // Whether the GuestView set an explicit zoom level.
  bool did_set_explicit_zoom_;

  // Store spatial navigation status.
  bool is_spatial_navigation_enabled_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<WebViewGuest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
