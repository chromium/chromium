// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_delegate.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/wake_lock/wake_lock_context_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/mhtml_generation_result.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/three_d_api_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_states.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_response_headers.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/frame/blocked_navigation_types.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_text_autosizer_page_info.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace base {
class FilePath;
}  // namespace base

namespace service_manager {
class InterfaceProvider;
}  // namespace service_manager

namespace content {
enum class PictureInPictureResult;
class BrowserPluginEmbedder;
class BrowserPluginGuest;
class DisplayCutoutHostImpl;
class FindRequestManager;
class InterstitialPageImpl;
class JavaScriptDialogManager;
class JavaScriptDialogNavigationDeferrer;
class ManifestManagerHost;
class MediaWebContentsObserver;
class PluginContentOriginWhitelist;
class Portal;
class RenderFrameHost;
class RenderViewHost;
class RenderViewHostDelegateView;
class RenderWidgetHostImpl;
class RenderWidgetHostInputEventRouter;
class SavePackage;
class ScreenOrientationProvider;
class SiteInstance;
class BeforeUnloadBlockingDelegate;  // content_browser_test_utils_internal.h
class
    TestWCDelegateForDialogsAndFullscreen;  // web_contents_impl_browsertest.cc
class TestWebContents;
class TextInputManager;
class WebContentsAudioMuter;
class WebContentsDelegate;
class WebContentsImpl;
class WebContentsView;
class WebContentsViewDelegate;
struct AXEventNotificationDetails;
struct FaviconURL;
struct LoadNotificationDetails;
struct MHTMLGenerationParams;

namespace mojom {
class CreateNewWindowParams;
}  // namespace mojom

#if defined(OS_ANDROID)
class WebContentsAndroid;
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
class PepperPlaybackObserver;
#endif

using AccessibilityEventCallback =
    base::RepeatingCallback<void(const std::string&)>;

// Factory function for the implementations that content knows about. Takes
// ownership of |delegate|.
WebContentsView* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view);

class CONTENT_EXPORT WebContentsImpl : public WebContents,
                                       public RenderFrameHostDelegate,
                                       public RenderViewHostDelegate,
                                       public RenderWidgetHostDelegate,
                                       public RenderFrameHostManager::Delegate,
                                       public blink::mojom::ColorChooserFactory,
                                       public NotificationObserver,
                                       public NavigationControllerDelegate,
                                       public NavigatorDelegate {
 public:
  class FriendWrapper;

  ~WebContentsImpl() override;

  static std::unique_ptr<WebContentsImpl> CreateWithOpener(
      const WebContents::CreateParams& params,
      RenderFrameHostImpl* opener_rfh);

  static std::vector<WebContentsImpl*> GetAllWebContents();

  static WebContentsImpl* FromFrameTreeNode(
      const FrameTreeNode* frame_tree_node);
  static WebContents* FromRenderFrameHostID(int render_process_host_id,
                                            int render_frame_host_id);
  static WebContents* FromFrameTreeNodeId(int frame_tree_node_id);
  static WebContentsImpl* FromOuterFrameTreeNode(
      const FrameTreeNode* frame_tree_node);

  // Complex initialization here. Specifically needed to avoid having
  // members call back into our virtual functions in the constructor.
  virtual void Init(const WebContents::CreateParams& params);

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

#if defined(OS_ANDROID)
  // In Android WebView, the RenderView needs created even there is no
  // navigation entry, this allows Android WebViews to use
  // javascript: URLs that load into the DOMWindow before the first page
  // load. This is not safe to do in any context that a web page could get a
  // reference to the DOMWindow before the first page load.
  bool CreateRenderViewForInitialEmptyDocument();
#endif

  // Expose the render manager for testing.
  // TODO(creis): Remove this now that we can get to it via FrameTreeNode.
  RenderFrameHostManager* GetRenderManagerForTesting();

  // Sets a BrowserPluginGuest object for this WebContents. If this WebContents
  // has a BrowserPluginGuest then that implies that it is being hosted by
  // a BrowserPlugin object in an embedder renderer process.
  void SetBrowserPluginGuest(std::unique_ptr<BrowserPluginGuest> guest);

  // Returns embedder browser plugin object, or NULL if this WebContents is not
  // an embedder.
  BrowserPluginEmbedder* GetBrowserPluginEmbedder() const;

  // Returns guest browser plugin object, or nullptr if this WebContents is not
  // for guest.
  BrowserPluginGuest* GetBrowserPluginGuest() const;

  // Creates a BrowserPluginEmbedder object for this WebContents if one doesn't
  // already exist.
  void CreateBrowserPluginEmbedderIfNecessary();

  // Cancels modal dialogs in this WebContents, as well as in any browser
  // plugins it is hosting.
  void CancelActiveAndPendingDialogs();

  // Informs the render view host and the BrowserPluginEmbedder, if present, of
  // a Drag Source End.
  void DragSourceEndedAt(float client_x,
                         float client_y,
                         float screen_x,
                         float screen_y,
                         blink::WebDragOperation operation,
                         RenderWidgetHost* source_rwh);

  // Notification that the RenderViewHost's load state changed.
  void LoadStateChanged(const std::string& host,
                        const net::LoadStateWithParam& load_state,
                        uint64_t upload_position,
                        uint64_t upload_size);

  // Updates the visibility and notifies observers. Note that this is
  // distinct from UpdateWebContentsVisibility which may also update the
  // visibility of renderer-side objects.
  void SetVisibilityAndNotifyObservers(Visibility visibility);

  // Notify observers that the web contents has been focused.
  void NotifyWebContentsFocused(RenderWidgetHost* render_widget_host);

  // Notify observers that the web contents has lost focus.
  void NotifyWebContentsLostFocus(RenderWidgetHost* render_widget_host);

  WebContentsView* GetView() const;

  void OnScreenOrientationChange();

  ScreenOrientationProvider* GetScreenOrientationProviderForTesting() const {
    return screen_orientation_provider_.get();
  }

  // Adds the given accessibility mode to the current accessibility mode
  // bitmap.
  void AddAccessibilityMode(ui::AXMode mode);

#if !defined(OS_ANDROID)
  // Sets the zoom level for frames associated with this WebContents.
  void UpdateZoom();

  // Sets the zoom level for frames associated with this WebContents if it
  // matches |host| and (if non-empty) |scheme|. Matching is done on the
  // last committed entry.
  void UpdateZoomIfNecessary(const std::string& scheme,
                             const std::string& host);

#endif  // !defined(OS_ANDROID)

  // Adds a new binding set to the WebContents. Returns a closure which may be
  // used to remove the binding set at any time. The closure is safe to call
  // even after WebContents destruction.
  //
  // |binding_set| is not owned and must either outlive this WebContents or be
  // explicitly removed before being destroyed.
  base::Closure AddBindingSet(const std::string& interface_name,
                              WebContentsBindingSet* binding_set);

  // Accesses a WebContentsBindingSet for a specific interface on this
  // WebContents. Returns null of there is no registered binder for the
  // interface.
  WebContentsBindingSet* GetBindingSet(const std::string& interface_name);

  // Returns the focused WebContents.
  // If there are multiple inner/outer WebContents (when embedding <webview>,
  // <guestview>, ...) returns the single one containing the currently focused
  // frame. Otherwise, returns this WebContents.
  WebContentsImpl* GetFocusedWebContents();

  // Returns a vector containing this WebContents and all inner WebContents
  // within it (recursively).
  std::vector<WebContentsImpl*> GetWebContentsAndAllInner();

  void NotifyManifestUrlChanged(const base::Optional<GURL>& manifest_url);

  ManifestManagerHost* GetManifestManagerHost() const {
    return manifest_manager_host_.get();
  }

#if defined(OS_ANDROID)
  void SetMainFrameImportance(ChildProcessImportance importance);
#endif

  // WebContents ------------------------------------------------------
  WebContentsDelegate* GetDelegate() override;
  void SetDelegate(WebContentsDelegate* delegate) override;
  NavigationControllerImpl& GetController() override;
  BrowserContext* GetBrowserContext() override;
  const GURL& GetURL() override;
  const GURL& GetVisibleURL() override;
  const GURL& GetLastCommittedURL() override;
  RenderFrameHostImpl* GetMainFrame() override;
  RenderFrameHostImpl* GetFocusedFrame() override;
  RenderFrameHostImpl* FindFrameByFrameTreeNodeId(int frame_tree_node_id,
                                                  int process_id) override;
  RenderFrameHostImpl* UnsafeFindFrameByFrameTreeNodeId(
      int frame_tree_node_id) override;
  void ForEachFrame(
      const base::RepeatingCallback<void(RenderFrameHost*)>& on_frame) override;
  std::vector<RenderFrameHost*> GetAllFrames() override;
  int SendToAllFrames(IPC::Message* message) override;
  RenderViewHostImpl* GetRenderViewHost() override;
  RenderWidgetHostView* GetRenderWidgetHostView() override;
  RenderWidgetHostView* GetTopLevelRenderWidgetHostView() override;
  void ClosePage() override;
  RenderWidgetHostView* GetFullscreenRenderWidgetHostView() override;
  base::Optional<SkColor> GetThemeColor() override;
  WebUI* GetWebUI() override;
  WebUI* GetCommittedWebUI() override;
  void SetUserAgentOverride(const std::string& override,
                            bool override_in_new_tabs) override;
  const std::string& GetUserAgentOverride() override;
  bool ShouldOverrideUserAgentInNewTabs() override;
  void EnableWebContentsOnlyAccessibilityMode() override;
  bool IsWebContentsOnlyAccessibilityModeForTesting() override;
  bool IsFullAccessibilityModeForTesting() override;
  const PageImportanceSignals& GetPageImportanceSignals() override;
  const base::string16& GetTitle() override;
  void UpdateTitleForEntry(NavigationEntry* entry,
                           const base::string16& title) override;
  SiteInstanceImpl* GetSiteInstance() override;
  bool IsLoading() override;
  double GetLoadProgress() override;
  bool IsLoadingToDifferentDocument() override;
  bool IsDocumentOnLoadCompletedInMainFrame() override;
  bool IsWaitingForResponse() override;
  const net::LoadStateWithParam& GetLoadState() override;
  const base::string16& GetLoadStateHost() override;
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                             ui::AXMode ax_mode) override;
  uint64_t GetUploadSize() override;
  uint64_t GetUploadPosition() override;
  const std::string& GetEncoding() override;
  bool WasDiscarded() override;
  void SetWasDiscarded(bool was_discarded) override;
  void IncrementCapturerCount(const gfx::Size& capture_size,
                              bool stay_hidden) override;
  void DecrementCapturerCount(bool stay_hidden) override;
  bool IsBeingCaptured() override;
  bool IsAudioMuted() override;
  void SetAudioMuted(bool mute) override;
  bool IsCurrentlyAudible() override;
  bool IsConnectedToBluetoothDevice() override;
  bool IsConnectedToSerialPort() override;
  bool IsConnectedToHidDevice() override;
  bool HasNativeFileSystemHandles() override;
  bool HasNativeFileSystemDirectoryHandles() override;
  std::vector<base::FilePath> GetNativeFileSystemDirectoryHandles() override;
  bool HasWritableNativeFileSystemHandles() override;
  bool HasPictureInPictureVideo() override;
  bool IsCrashed() override;
  void SetIsCrashed(base::TerminationStatus status, int error_code) override;
  base::TerminationStatus GetCrashedStatus() override;
  int GetCrashedErrorCode() override;
  bool IsBeingDestroyed() override;
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override;
  void OnAudioStateChanged() override;
  base::TimeTicks GetLastActiveTime() override;
  void WasShown() override;
  void WasHidden() override;
  void WasOccluded() override;
  Visibility GetVisibility() override;
  bool NeedToFireBeforeUnloadOrUnload() override;
  void DispatchBeforeUnload(bool auto_cancel) override;
  void AttachInnerWebContents(std::unique_ptr<WebContents> inner_web_contents,
                              RenderFrameHost* render_frame_host,
                              bool is_full_page) override;
  RenderFrameHostImpl* GetOuterWebContentsFrame() override;
  WebContentsImpl* GetOuterWebContents() override;
  WebContentsImpl* GetOutermostWebContents() override;
  std::vector<WebContents*> GetInnerWebContents() override;
  void DidChangeVisibleSecurityState() override;
  void NotifyPreferencesChanged() override;
  void SyncRendererPrefs() override;
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChange(const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;

  void Stop() override;
  void SetPageFrozen(bool frozen) override;
  std::unique_ptr<WebContents> Clone() override;
  void ReloadFocusedFrame() override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void CopyToFindPboard() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void Delete() override;
  void SelectAll() override;
  void CollapseSelection() override;
  void Replace(const base::string16& word) override;
  void ReplaceMisspelling(const base::string16& word) override;
  void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) override;
  std::vector<mojo::Remote<blink::mojom::PauseSubresourceLoadingHandle>>
  PauseSubresourceLoading() override;
  void ExecuteCustomContextMenuCommand(
      int action,
      const CustomContextMenuContext& context) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeView GetContentNativeView() override;
  gfx::NativeWindow GetTopLevelNativeWindow() override;
  gfx::Rect GetContainerBounds() override;
  gfx::Rect GetViewBounds() override;
  DropData* GetDropData() override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  void FocusThroughTabTraversal(bool reverse) override;
  bool ShowingInterstitialPage() override;
  InterstitialPageImpl* GetInterstitialPage() override;
  bool IsSavable() override;
  void OnSavePage() override;
  bool SavePage(const base::FilePath& main_file,
                const base::FilePath& dir_path,
                SavePageType save_type) override;
  void SaveFrame(const GURL& url, const Referrer& referrer) override;
  void SaveFrameWithHeaders(const GURL& url,
                            const Referrer& referrer,
                            const std::string& headers,
                            const base::string16& suggested_filename) override;
  void GenerateMHTML(const MHTMLGenerationParams& params,
                     base::OnceCallback<void(int64_t)> callback) override;
  void GenerateMHTMLWithResult(
      const MHTMLGenerationParams& params,
      MHTMLGenerationResult::GenerateMHTMLCallback callback) override;
  const std::string& GetContentsMimeType() override;
  bool WillNotifyDisconnection() override;
  blink::mojom::RendererPreferences* GetMutableRendererPrefs() override;
  void Close() override;
  void SystemDragEnded(RenderWidgetHost* source_rwh) override;
  void SetClosedByUserGesture(bool value) override;
  bool GetClosedByUserGesture() override;
  int GetMinimumZoomPercent() override;
  int GetMaximumZoomPercent() override;
  void SetPageScale(float page_scale_factor) override;
  gfx::Size GetPreferredSize() override;
  bool GotResponseToLockMouseRequest(bool allowed) override;
  bool GotResponseToKeyboardLockRequest(bool allowed) override;
  bool HasOpener() override;
  RenderFrameHostImpl* GetOpener() override;
  bool HasOriginalOpener() override;
  RenderFrameHostImpl* GetOriginalOpener() override;
  void DidChooseColorInColorChooser(SkColor color) override;
  void DidEndColorChooser() override;
  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    uint32_t preferred_size,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    ImageDownloadCallback callback) override;
  void Find(int request_id,
            const base::string16& search_text,
            blink::mojom::FindOptionsPtr options) override;
  void StopFinding(StopFindAction action) override;
  bool WasEverAudible() override;
  void GetManifest(GetManifestCallback callback) override;
  bool IsFullscreenForCurrentTab() override;
  bool ShouldShowStaleContentOnEviction() override;
  void ExitFullscreen(bool will_cause_resize) override;
  void ForSecurityDropFullscreen() override;
  void ResumeLoadingCreatedWebContents() override;
  void SetIsOverlayContent(bool is_overlay_content) override;
  bool IsFocusedElementEditable() override;
  void ClearFocusedElement() override;
  bool IsShowingContextMenu() override;
  void SetShowingContextMenu(bool showing) override;
  base::UnguessableToken GetAudioGroupId() override;
  bool CompletedFirstVisuallyNonEmptyPaint() override;
  ukm::SourceId GetLastCommittedSourceId() override;

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents() override;
  WebContentsAndroid* GetWebContentsAndroid();
  void ClearWebContentsAndroid();
  void ActivateNearestFindResult(float x, float y) override;
  void RequestFindMatchRects(int current_version) override;
  service_manager::InterfaceProvider* GetJavaInterfaces() override;
#endif

  bool HasRecentInteractiveInputEvent() override;
  void SetIgnoreInputEvents(bool ignore_input_events) override;

  // Implementation of PageNavigator.
  WebContents* OpenURL(const OpenURLParams& params) override;

  // RenderFrameHostDelegate ---------------------------------------------------
  bool OnMessageReceived(RenderFrameHostImpl* render_frame_host,
                         const IPC::Message& message) override;
  void OnAssociatedInterfaceRequest(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;
  void OnInterfaceRequest(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void OnDidBlockNavigation(const GURL& blocked_url,
                            const GURL& initiator_url,
                            blink::NavigationBlockedReason reason) override;
  const GURL& GetMainFrameLastCommittedURL() override;
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;
  void RunJavaScriptDialog(RenderFrameHost* render_frame_host,
                           const base::string16& message,
                           const base::string16& default_prompt,
                           JavaScriptDialogType dialog_type,
                           IPC::Message* reply_msg) override;
  void RunBeforeUnloadConfirm(RenderFrameHost* render_frame_host,
                              bool is_reload,
                              IPC::Message* reply_msg) override;
  void RunFileChooser(RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void EnumerateDirectory(RenderFrameHost* render_frame_host,
                          std::unique_ptr<FileSelectListener> listener,
                          const base::FilePath& directory_path) override;
  void DidCancelLoading() override;
  void DidAccessInitialDocument() override;
  void DidChangeName(RenderFrameHost* render_frame_host,
                     const std::string& name) override;
  void DidReceiveFirstUserActivation(
      RenderFrameHost* render_frame_host) override;
  void DidChangeDisplayState(RenderFrameHost* render_frame_host,
                             bool is_display_none) override;
  void FrameSizeChanged(RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void DOMContentLoaded(RenderFrameHost* render_frame_host) override;
  void DocumentOnLoadCompleted(RenderFrameHost* render_frame_host) override;
  void UpdateStateForFrame(RenderFrameHost* render_frame_host,
                           const PageState& page_state) override;
  void UpdateTitle(RenderFrameHost* render_frame_host,
                   const base::string16& title,
                   base::i18n::TextDirection title_direction) override;
  WebContents* GetAsWebContents() override;
  bool IsNeverVisible() override;
  ui::AXMode GetAccessibilityMode() override;
  // Broadcasts the mode change to all frames.
  void SetAccessibilityMode(ui::AXMode mode) override;
  void AccessibilityEventReceived(
      const AXEventNotificationDetails& details) override;
  void AccessibilityLocationChangesReceived(
      const std::vector<AXLocationChangeNotificationDetails>& details) override;
  base::string16 DumpAccessibilityTree(
      bool internal,
      std::vector<content::AccessibilityTreeFormatter::PropertyFilter>
          property_filters) override;
  void RecordAccessibilityEvents(AccessibilityEventCallback callback,
                                 bool start) override;
  RenderFrameHost* GetGuestByInstanceID(
      RenderFrameHost* render_frame_host,
      int browser_plugin_instance_id) override;
  device::mojom::GeolocationContext* GetGeolocationContext() override;
  device::mojom::WakeLockContext* GetWakeLockContext() override;
#if defined(OS_ANDROID)
  void GetNFC(mojo::PendingReceiver<device::mojom::NFC> receiver) override;
#endif
  void EnterFullscreenMode(
      const GURL& origin,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenMode(bool will_cause_resize) override;
  void FullscreenStateChanged(RenderFrameHost* rfh,
                              bool is_fullscreen) override;
#if defined(OS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override;
#endif
  bool ShouldRouteMessageEvent(
      RenderFrameHost* target_rfh,
      SiteInstance* source_site_instance) const override;
  void EnsureOpenerProxiesExist(RenderFrameHost* source_rfh) override;
  std::unique_ptr<WebUIImpl> CreateWebUIForRenderFrameHost(
      const GURL& url) override;
  void SetFocusedFrame(FrameTreeNode* node, SiteInstance* source) override;
  void DidCallFocus() override;
  RenderFrameHost* GetFocusedFrameIncludingInnerWebContents() override;
  void OnFocusedElementChangedInFrame(
      RenderFrameHostImpl* frame,
      const gfx::Rect& bounds_in_root_view) override;
  void OnAdvanceFocus(RenderFrameHostImpl* source_rfh) override;
  RenderFrameHostDelegate* CreateNewWindow(
      RenderFrameHost* opener,
      const mojom::CreateNewWindowParams& params,
      bool is_new_browsing_instance,
      bool has_user_gesture,
      SessionStorageNamespace* session_storage_namespace) override;
  void ShowCreatedWindow(int process_id,
                         int main_frame_widget_route_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) override;
  void DidDisplayInsecureContent() override;
  void DidContainInsecureFormAction() override;
  void DidRunInsecureContent(const GURL& security_origin,
                             const GURL& target_url) override;
  void PassiveInsecureContentFound(const GURL& resource_url) override;
  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  void ViewSource(RenderFrameHostImpl* frame) override;
  void PrintCrossProcessSubframe(const gfx::Rect& rect,
                                 int document_cookie,
                                 RenderFrameHost* render_frame_host) override;
#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHostDelegate()
      override;
#endif
  void SubresourceResponseStarted(
      const url::Origin& origin_of_final_response_url,
      net::CertStatus cert_status) override;
  void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      mojom::ResourceLoadInfoPtr resource_load_information) override;

  // Called when WebAudio starts or stops playing audible audio in an
  // AudioContext.
  void AudioContextPlaybackStarted(RenderFrameHost* host,
                                   int context_id) override;
  void AudioContextPlaybackStopped(RenderFrameHost* host,
                                   int context_id) override;
  void MediaWatchTimeChanged(
      const content::MediaPlayerWatchTime& watch_time) override;
  media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
  GetRecordAggregateWatchTimeCallback() override;
  RenderFrameHostImpl* GetMainFrameForInnerDelegate(
      FrameTreeNode* frame_tree_node) override;
  bool IsFrameLowPriority(const RenderFrameHost* render_frame_host) override;
  void RegisterProtocolHandler(RenderFrameHostImpl* source,
                               const std::string& protocol,
                               const GURL& url,
                               const base::string16& title,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(RenderFrameHostImpl* source,
                                 const std::string& protocol,
                                 const GURL& url,
                                 bool user_gesture) override;

  // RenderViewHostDelegate ----------------------------------------------------
  RenderViewHostDelegateView* GetDelegateView() override;
  bool OnMessageReceived(RenderViewHostImpl* render_view_host,
                         const IPC::Message& message) override;
  // RenderFrameHostDelegate has the same method, so list it there because this
  // interface is going away.
  // WebContents* GetAsWebContents() override;
  void RenderViewCreated(RenderViewHost* render_view_host) override;
  void RenderViewReady(RenderViewHost* render_view_host) override;
  void RenderViewTerminated(RenderViewHost* render_view_host,
                            base::TerminationStatus status,
                            int error_code) override;
  void RenderViewDeleted(RenderViewHost* render_view_host) override;
  void UpdateTargetURL(RenderViewHost* render_view_host,
                       const GURL& url) override;
  void Close(RenderViewHost* render_view_host) override;
  void RequestSetBounds(const gfx::Rect& new_bounds) override;
  void DocumentAvailableInMainFrame(RenderViewHost* render_view_host) override;
  void RouteCloseEvent(RenderViewHost* rvh) override;
  bool DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  blink::mojom::RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const override;
  void DidReceiveInputEvent(RenderWidgetHostImpl* render_widget_host,
                            const blink::WebInputEvent::Type type) override;
  bool ShouldIgnoreInputEvents() override;
  void OnIgnoredUIEvent() override;
  void Activate() override;
  void UpdatePreferredSize(const gfx::Size& pref_size) override;
  void CreateNewWidget(int32_t render_process_id,
                       int32_t route_id,
                       mojo::PendingRemote<mojom::Widget> widget,
                       RenderViewHostImpl* render_view_host) override;
  void CreateNewFullscreenWidget(int32_t render_process_id,
                                 int32_t widget_route_id,
                                 mojo::PendingRemote<mojom::Widget> widget,
                                 RenderViewHostImpl* render_view_host) override;
  void ShowCreatedWidget(int process_id,
                         int widget_route_id,
                         const gfx::Rect& initial_rect) override;
  void ShowCreatedFullscreenWidget(int process_id,
                                   int widget_route_id) override;
  void RequestMediaAccessPermission(const MediaStreamRequest& request,
                                    MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  std::string GetDefaultMediaDeviceID(
      blink::mojom::MediaStreamType type) override;
  SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance) override;
  SessionStorageNamespaceMap GetSessionStorageNamespaceMap() override;
  FrameTree* GetFrameTree() override;
  bool IsOverridingUserAgent() override;
  bool IsJavaScriptDialogShowing() const override;
  bool ShouldIgnoreUnresponsiveRenderer() override;
  bool HideDownloadUI() const override;
  bool HasPersistentVideo() const override;
  bool IsSpatialNavigationDisabled() const override;
  RenderFrameHostImpl* GetPendingMainFrame() override;
  void DidFirstVisuallyNonEmptyPaint(RenderViewHostImpl* source) override;
  bool IsPortal() const override;
  void OnThemeColorChanged(RenderViewHostImpl* source) override;

  // NavigatorDelegate ---------------------------------------------------------

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                            const GURL& url,
                            int error_code,
                            const base::string16& error_description) override;
  void DidNavigateMainFramePreCommit(bool navigation_is_within_page) override;
  void DidNavigateMainFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) override;
  void DidNavigateAnyFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) override;
  void SetMainFrameMimeType(const std::string& mime_type) override;
  bool CanOverscrollContent() const override;
  void NotifyChangedNavigationState(InvalidateTypes changed_flags) override;
  void DidStartNavigationToPendingEntry(const GURL& url,
                                        ReloadType reload_type) override;
  bool ShouldTransferNavigation(bool is_main_frame_navigation) override;
  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool to_different_document) override;
  void DidStopLoading() override;
  void DidChangeLoadProgress() override;
  std::vector<std::unique_ptr<NavigationThrottle>> CreateThrottlesForNavigation(
      NavigationHandle* navigation_handle) override;
  std::unique_ptr<NavigationUIData> GetNavigationUIData(
      NavigationHandle* navigation_handle) override;

  // RenderWidgetHostDelegate --------------------------------------------------

  ukm::SourceId GetUkmSourceIdForLastCommittedSource() const override;
  ukm::SourceId GetUkmSourceIdForLastCommittedSourceIncludingSameDocument()
      const override;
  void SetTopControlsShownRatio(RenderWidgetHostImpl* render_widget_host,
                                float ratio) override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  void RenderWidgetCreated(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetDeleted(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetGotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetLostFocus(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetWasResized(RenderWidgetHostImpl* render_widget_host,
                              bool width_changed) override;
  void ResizeDueToAutoResize(RenderWidgetHostImpl* render_widget_host,
                             const gfx::Size& new_size) override;
  gfx::Size GetAutoResizeSize() override;
  void ResetAutoResizeSize() override;
  InputEventShim* GetInputEventShim() const override;
  RenderFrameHostImpl* GetFocusedFrameFromFocusedDelegate() override;
  void OnVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection scroll_direction) override;

#if !defined(OS_ANDROID)
  double GetPendingPageZoomLevel() override;
#endif  // !defined(OS_ANDROID)

  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) override;
  bool HandleMouseEvent(const blink::WebMouseEvent& event) override;
  bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event) override;
  bool HandleWheelEvent(const blink::WebMouseWheelEvent& event) override;
  bool PreHandleGestureEvent(const blink::WebGestureEvent& event) override;
  BrowserAccessibilityManager* GetRootBrowserAccessibilityManager() override;
  BrowserAccessibilityManager* GetOrCreateRootBrowserAccessibilityManager()
      override;
  // The following 4 functions are already listed under WebContents overrides:
  // void Cut() override;
  // void Copy() override;
  // void Paste() override;
  // void SelectAll() override;
  void ExecuteEditCommand(const std::string& command,
                          const base::Optional<base::string16>& value) override;
  void MoveRangeSelectionExtent(const gfx::Point& extent) override;
  void SelectRange(const gfx::Point& base, const gfx::Point& extent) override;
  void MoveCaret(const gfx::Point& extent) override;
  void AdjustSelectionByCharacterOffset(int start_adjust,
                                        int end_adjust,
                                        bool show_selection_menu) override;
  RenderWidgetHostInputEventRouter* GetInputEventRouter() override;
  void ReplicatePageFocus(bool is_focused) override;
  RenderWidgetHostImpl* GetFocusedRenderWidgetHost(
      RenderWidgetHostImpl* receiving_widget) override;
  RenderWidgetHostImpl* GetRenderWidgetHostWithPageFocus() override;
  void FocusOwningWebContents(
      RenderWidgetHostImpl* render_widget_host) override;
  void RendererUnresponsive(
      RenderWidgetHostImpl* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void RendererResponsive(RenderWidgetHostImpl* render_widget_host) override;
  void SubframeCrashed(blink::mojom::FrameVisibility visibility) override;
  void RequestToLockMouse(RenderWidgetHostImpl* render_widget_host,
                          bool user_gesture,
                          bool last_unlocked_by_target,
                          bool privileged) override;
  bool RequestKeyboardLock(RenderWidgetHostImpl* render_widget_host,
                           bool esc_key_locked) override;
  void CancelKeyboardLock(RenderWidgetHostImpl* render_widget_host) override;
  RenderWidgetHostImpl* GetKeyboardLockWidget() override;
  // The following function is already listed under WebContents overrides:
  // bool IsFullscreenForCurrentTab() const override;
  blink::mojom::DisplayMode GetDisplayMode(
      RenderWidgetHostImpl* render_widget_host) const override;
  void LostCapture(RenderWidgetHostImpl* render_widget_host) override;
  void LostMouseLock(RenderWidgetHostImpl* render_widget_host) override;
  bool HasMouseLock(RenderWidgetHostImpl* render_widget_host) override;
  RenderWidgetHostImpl* GetMouseLockWidget() override;
  void OnRenderFrameProxyVisibilityChanged(
      blink::mojom::FrameVisibility visibility) override;
  void SendScreenRects() override;
  TextInputManager* GetTextInputManager() override;
  bool OnUpdateDragCursor() override;
  bool IsWidgetForMainFrame(RenderWidgetHostImpl* render_widget_host) override;
  bool AddDomainInfoToRapporSample(rappor::Sample* sample) override;
  bool IsShowingContextMenuOnPage() const override;

  // RenderFrameHostManager::Delegate ------------------------------------------

  bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host,
      int opener_frame_routing_id,
      int proxy_routing_id,
      const base::UnguessableToken& devtools_frame_token,
      const FrameReplicationState& replicated_frame_state) override;
  void CreateRenderWidgetHostViewForRenderManager(
      RenderViewHost* render_view_host) override;
  bool CreateRenderFrameForRenderManager(
      RenderFrameHost* render_frame_host,
      int previous_routing_id,
      int opener_routing_id,
      int parent_routing_id,
      int previous_sibling_routing_id) override;
  void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      const base::TimeTicks& proceed_time,
      bool* proceed_to_fire_unload) override;
  void RenderProcessGoneFromRenderManager(
      RenderViewHost* render_view_host) override;
  void UpdateRenderViewSizeForRenderManager(bool is_main_frame) override;
  void CancelModalDialogsForRenderManager() override;
  void NotifySwappedFromRenderManager(RenderFrameHost* old_host,
                                      RenderFrameHost* new_host,
                                      bool is_main_frame) override;
  void NotifyMainFrameSwappedFromRenderManager(
      RenderFrameHost* old_host,
      RenderFrameHost* new_host) override;
  NavigationControllerImpl& GetControllerForRenderManager() override;
  InterstitialPageImpl* GetInterstitialForRenderManager() override;
  bool FocusLocationBarByDefault() override;
  void SetFocusToLocationBar() override;
  bool IsHidden() override;
  int GetOuterDelegateFrameTreeNodeId() override;
  RenderWidgetHostImpl* GetFullscreenRenderWidgetHost() const override;

  // blink::mojom::ColorChooserFactory ---------------------------------------

  void OnColorChooserFactoryReceiver(
      mojo::PendingReceiver<blink::mojom::ColorChooserFactory> receiver);
  void OpenColorChooser(
      mojo::PendingReceiver<blink::mojom::ColorChooser> chooser,
      mojo::PendingRemote<blink::mojom::ColorChooserClient> client,
      SkColor color,
      std::vector<blink::mojom::ColorSuggestionPtr> suggestions) override;

  // NotificationObserver ------------------------------------------------------

  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // NavigationControllerDelegate ----------------------------------------------

  WebContents* GetWebContents() override;
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override;
  void NotifyNavigationListPruned(const PrunedDetails& pruned_details) override;
  void NotifyNavigationEntriesDeleted() override;
  bool ShouldPreserveAbortedURLs() override;

  // Invoked before a form repost warning is shown.
  void NotifyBeforeFormRepostWarningShow() override;

  // Activate this WebContents and show a form repost warning.
  void ActivateAndShowRepostFormWarningDialog() override;

  // Whether the initial empty page of this view has been accessed by another
  // page, making it unsafe to show the pending URL. Always false after the
  // first commit.
  bool HasAccessedInitialDocument() override;

  // Sets the history for this WebContentsImpl to |history_length| entries, with
  // an offset of |history_offset|.  This notifies all renderers involved in
  // rendering the current page about the new offset and length.
  void SetHistoryOffsetAndLength(int history_offset,
                                 int history_length) override;

  // Called by InterstitialPageImpl when it creates a RenderFrameHost.
  void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) override;

  // Sets the passed interstitial as the currently showing interstitial.
  // No interstitial page should already be attached.
  void AttachInterstitialPage(InterstitialPageImpl* interstitial_page) override;

  void MediaMutedStatusChanged(const MediaPlayerId& id, bool muted);

  // Unsets the currently showing interstitial.
  void DetachInterstitialPage(bool has_focus) override;

  void UpdateOverridingUserAgent() override;

  // Unpause the throbber if it was paused.
  void DidProceedOnInterstitial() override;

  bool HadInnerWebContents() override;

  // Forces overscroll to be disabled (used by touch emulation).
  void SetForceDisableOverscrollContent(bool force_disable);

  // Override the render view/widget size of the main frame, return whether the
  // size changed.
  bool SetDeviceEmulationSize(const gfx::Size& new_size);
  void ClearDeviceEmulationSize();

  AudioStreamMonitor* audio_stream_monitor() {
    return &audio_stream_monitor_;
  }

  ForwardingAudioStreamFactory* GetAudioStreamFactory();

  // Called by MediaWebContentsObserver when playback starts or stops.  See the
  // WebContentsObserver function stubs for more details.
  void MediaStartedPlaying(
      const WebContentsObserver::MediaPlayerInfo& media_info,
      const MediaPlayerId& id);
  void MediaStoppedPlaying(
      const WebContentsObserver::MediaPlayerInfo& media_info,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason);
  // This will be called before playback is started, check
  // GetCurrentlyPlayingVideoCount if you need this when playback starts.
  void MediaResized(const gfx::Size& size, const MediaPlayerId& id);
  void MediaEffectivelyFullscreenChanged(bool is_fullscreen);

  int GetCurrentlyPlayingVideoCount() override;
  base::Optional<gfx::Size> GetFullscreenVideoSize() override;
  bool IsFullscreen() override;

  MediaWebContentsObserver* media_web_contents_observer() {
    return media_web_contents_observer_.get();
  }

  // Update the web contents visibility.
  void UpdateWebContentsVisibility(Visibility visibility);

  // Called by FindRequestManager when find replies come in from a renderer
  // process.
  void NotifyFindReply(int request_id,
                       int number_of_matches,
                       const gfx::Rect& selection_rect,
                       int active_match_ordinal,
                       bool final_update);

  // Modify the counter of connected devices for this WebContents.
  void IncrementBluetoothConnectedDeviceCount();
  void DecrementBluetoothConnectedDeviceCount();

  // Modify the counter of frames in this WebContents actively using serial
  // ports.
  void IncrementSerialActiveFrameCount();
  void DecrementSerialActiveFrameCount();

  // Modify the counter of frames in this WebContents actively using HID
  // devices.
  void IncrementHidActiveFrameCount();
  void DecrementHidActiveFrameCount();

  // Modify the counter of native file system handles for this WebContents.
  void IncrementNativeFileSystemHandleCount();
  void DecrementNativeFileSystemHandleCount();

  // Add and remove a reference for a native file system directory handle for a
  // certain |path|. Multiple Add calls should be balanced by the same number of
  // Remove calls for the same |path|.
  void AddNativeFileSystemDirectoryHandle(const base::FilePath& path);
  void RemoveNativeFileSystemDirectoryHandle(const base::FilePath& path);

  // Modify the counter of native file system handles with write access for this
  // WebContents.
  void IncrementWritableNativeFileSystemHandleCount();
  void DecrementWritableNativeFileSystemHandleCount();

  // Called when the WebContents gains or loses a persistent video.
  void SetHasPersistentVideo(bool has_persistent_video);

  // Whether the WebContents has an active player is effectively fullscreen.
  // That means that the video is either fullscreen or it is the content of
  // a fullscreen page (in other words, a fullscreen video with custom
  // controls).
  // |IsFullscreen| must return |true| when this method is called.
  bool HasActiveEffectivelyFullscreenVideo() const;

  // Whether the WebContents effectively fullscreen active player allows
  // Picture-in-Picture.
  // |IsFullscreen| must return |true| when this method is called.
  bool IsPictureInPictureAllowedForFullscreenVideo() const;

  // When inner or outer WebContents are present, become the focused
  // WebContentsImpl. This will activate this content's main frame RenderWidget
  // and indirectly all its subframe widgets.  GetFocusedRenderWidgetHost will
  // search this WebContentsImpl for a focused RenderWidgetHost. The previously
  // focused WebContentsImpl, if any, will have its RenderWidgetHosts
  // deactivated.
  void SetAsFocusedWebContentsIfNecessary();

  // Called by this WebContents's BrowserPluginGuest (if one exists) to indicate
  // that the guest will be detached.
  void BrowserPluginGuestWillDetach();

  // Notifies the Picture-in-Picture controller that there is a new player
  // entering Picture-in-Picture.
  // Returns the result of the enter request.
  PictureInPictureResult EnterPictureInPicture(const viz::SurfaceId&,
                                               const gfx::Size& natural_size);

  // Updates the Picture-in-Picture controller with a signal that
  // Picture-in-Picture mode has ended.
  void ExitPictureInPicture();

  // Updates the tracking information for |this| to know if there is
  // a video currently in Picture-in-Picture mode.
  void SetHasPictureInPictureVideo(bool has_picture_in_picture_video);

  // Sets the spatial navigation state.
  void SetSpatialNavigationDisabled(bool disabled);

#if defined(OS_ANDROID)
  // Called by FindRequestManager when all of the find match rects are in.
  void NotifyFindMatchRectsReply(int version,
                                 const std::vector<gfx::RectF>& rects,
                                 const gfx::RectF& active_rect);
#endif

#if defined(OS_ANDROID)
  // Called by WebContentsAndroid to send the Display Cutout safe area to
  // DisplayCutoutHostImpl.
  void SetDisplayCutoutSafeArea(gfx::Insets insets);
#endif

  // Notify observers that the viewport fit value changed. This is called by
  // |DisplayCutoutHostImpl|.
  void NotifyViewportFitChanged(blink::mojom::ViewportFit value);

  // Returns the current FindRequestManager associated with the WebContents;
  // this won't create one if none exists.
  FindRequestManager* GetFindRequestManagerForTesting();

  // Detaches this WebContents from its outer WebContents.
  std::unique_ptr<WebContents> DetachFromOuterWebContents();

  // Reattaches this inner WebContents to its outer WebContents.
  void ReattachToOuterWebContentsFrame();

  // Getter/setter for the Portal associated with this WebContents. If non-null
  // then this WebContents is embedded in a portal and its outer WebContents can
  // be found by using GetOuterWebContents().
  void set_portal(Portal* portal) { portal_ = portal; }
  Portal* portal() const { return portal_; }

  // Notifies observers that AppCache was accessed. Public so AppCache code can
  // call this directly.
  void OnAppCacheAccessed(const GURL& manifest_url, bool blocked_by_policy);

  JavaScriptDialogNavigationDeferrer* GetJavaScriptDialogNavigationDeferrer() {
    return javascript_dialog_navigation_deferrer_.get();
  }

  // Returns the focused frame's input handler.
  mojom::FrameInputHandler* GetFocusedFrameInputHandler();

 private:
  friend class WebContentsObserver;
  friend class WebContents;  // To implement factory methods.

  friend class RenderFrameHostImplBeforeUnloadBrowserTest;
  friend class WebContentsImplBrowserTest;
  friend class BeforeUnloadBlockingDelegate;
  friend class TestWCDelegateForDialogsAndFullscreen;

  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, NoJSMessageOnInterstitials);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, UpdateTitle);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FindOpenerRVHWhenPending);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           CrossSiteCantPreemptAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, PendingContentsDestroyed);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, PendingContentsShown);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FrameTreeShape);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, GetLastActiveTime);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           LoadResourceFromMemoryCacheWithBadSecurityInfo);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           LoadResourceWithEmptySecurityInfo);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           ResetJavaScriptDialogOnUserNavigate);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, ParseDownloadHeaders);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           NotifyFullscreenAcquired);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           NotifyFullscreenAcquired_Navigate);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           NotifyFullscreenAcquired_SameOrigin);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           FullscreenAfterFrameSwap);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           MaxFrameCountForCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           MaxFrameCountRemovedIframes);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           MaxFrameCountInjectedIframes);
  FRIEND_TEST_ALL_PREFIXES(FormStructureBrowserTest, HTMLFiles);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest, HistoryNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, PageDoesBackAndReload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrossSiteIframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           TwoSubframesCreatePopupsSimultaneously);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           TwoSubframesCreatePopupMenuWidgetsSimultaneously);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessAccessibilityBrowserTest,
                           CrossSiteIframeAccessibility);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           IframeBeforeUnloadParentHang);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           BeforeUnloadDialogRequiresGesture);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           CancelBeforeUnloadResetsURL);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           BeforeUnloadDialogSuppressedForDiscard);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           PendingDialogMakesDiscardUnloadReturnFalse);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, JavaScriptDialogNotifications);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, JavaScriptDialogInterop);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, BeforeUnloadDialog);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, PageDisableWithOpenedDialog);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest,
                           PageDisableWithNoDialogManager);
  FRIEND_TEST_ALL_PREFIXES(PointerLockBrowserTest,
                           PointerLockInnerContentsCrashes);
  FRIEND_TEST_ALL_PREFIXES(PointerLockBrowserTest, PointerLockOopifCrashes);

  // So |find_request_manager_| can be accessed for testing.
  friend class FindRequestManagerTest;

  // TODO(brettw) TestWebContents shouldn't exist!
  friend class TestWebContents;

  class DestructionObserver;

  // Represents a WebContents node in a tree of WebContents structure.
  //
  // Two WebContents with separate FrameTrees can be connected by
  // outer/inner relationship using this class. Note that their FrameTrees
  // still remain disjoint.
  // The parent is referred to as "outer WebContents" and the descendents are
  // referred to as "inner WebContents".
  // For each inner WebContents, the outer WebContents will have a
  // corresponding FrameTreeNode.
  class WebContentsTreeNode final : public FrameTreeNode::Observer {
   public:
    explicit WebContentsTreeNode(WebContentsImpl* current_web_contents);
    ~WebContentsTreeNode() final;

    // Attaches |inner_web_contents| to the |render_frame_host| within this
    // WebContents.
    void AttachInnerWebContents(std::unique_ptr<WebContents> inner_web_contents,
                                RenderFrameHostImpl* render_frame_host);

    // Disconnects the current WebContents from its outer WebContents, and
    // returns ownership to the caller. This is used when activating a portal,
    // which causes the WebContents to transition from an inner WebContents to
    // an outer WebContents.
    // TODO(lfg): Activating a nested portal currently replaces the outermost
    // WebContents with the portal. We should allow replacing only the inner
    // WebContents with the nested portal.
    std::unique_ptr<WebContents> DisconnectFromOuterWebContents();

    WebContentsImpl* outer_web_contents() const { return outer_web_contents_; }
    int outer_contents_frame_tree_node_id() const {
      return outer_contents_frame_tree_node_id_;
    }
    FrameTreeNode* OuterContentsFrameTreeNode() const;

    WebContentsImpl* focused_web_contents() { return focused_web_contents_; }
    void SetFocusedWebContents(WebContentsImpl* web_contents);

    // Returns the inner WebContents within |frame|, if one exists, or nullptr
    // otherwise.
    WebContentsImpl* GetInnerWebContentsInFrame(const FrameTreeNode* frame);

    std::vector<WebContentsImpl*> GetInnerWebContents() const;

   private:
    std::unique_ptr<WebContents> DetachInnerWebContents(
        WebContentsImpl* inner_web_contents);

    // FrameTreeNode::Observer implementation.
    void OnFrameTreeNodeDestroyed(FrameTreeNode* node) final;

    // The WebContents that owns this WebContentsTreeNode.
    WebContentsImpl* const current_web_contents_;

    // The outer WebContents of |current_web_contents_|, or nullptr if
    // |current_web_contents_| is the outermost WebContents.
    WebContentsImpl* outer_web_contents_;

    // The ID of the FrameTreeNode in the |outer_web_contents_| that hosts
    // |current_web_contents_| as an inner WebContents.
    int outer_contents_frame_tree_node_id_;

    // List of inner WebContents that we host. The outer WebContents owns the
    // inner WebContents.
    std::vector<std::unique_ptr<WebContents>> inner_web_contents_;

    // Only the root node should have this set. This indicates the WebContents
    // whose frame tree has the focused frame. The WebContents tree could be
    // arbitrarily deep.
    WebContentsImpl* focused_web_contents_;
  };

  // See WebContents::Create for a description of these parameters.
  explicit WebContentsImpl(BrowserContext* browser_context);

  // Covariant return type alternative for WebContents::Create(). Avoids
  // need for casting of objects inside the content layer.
  static std::unique_ptr<WebContentsImpl> Create(const CreateParams& params);

  // Add and remove observers for page navigation notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  void AddObserver(WebContentsObserver* observer);
  void RemoveObserver(WebContentsObserver* observer);

  // Clears a pending contents that has been closed before being shown.
  void OnWebContentsDestroyed(WebContentsImpl* web_contents);

  // Creates and adds to the map a destruction observer watching |web_contents|.
  // No-op if such an observer already exists.
  void AddDestructionObserver(WebContentsImpl* web_contents);

  // Deletes and removes from the map a destruction observer
  // watching |web_contents|. No-op if there is no such observer.
  void RemoveDestructionObserver(WebContentsImpl* web_contents);

  // Traverses all the RenderFrameHosts in the FrameTree and creates a set
  // all the unique RenderWidgetHostViews.
  std::set<RenderWidgetHostView*> GetRenderWidgetHostViewsInTree();

  // Called with the result of a DownloadImage() request.
  void OnDidDownloadImage(ImageDownloadCallback callback,
                          int id,
                          const GURL& image_url,
                          int32_t http_status_code,
                          const std::vector<SkBitmap>& images,
                          const std::vector<gfx::Size>& original_image_sizes);

  // Callback function when showing JavaScript dialogs. Takes in a routing ID
  // pair to identify the RenderFrameHost that opened the dialog, because it's
  // possible for the RenderFrameHost to be deleted by the time this is called.
  void OnDialogClosed(int render_process_id,
                      int render_frame_id,
                      IPC::Message* reply_msg,
                      bool dialog_was_suppressed,
                      bool success,
                      const base::string16& user_input);

  // IPC message handlers.
  void OnDidLoadResourceFromMemoryCache(
      RenderFrameHostImpl* source,
      const GURL& url,
      const std::string& http_request,
      const std::string& mime_type,
      ResourceType resource_type);
  void OnDidRunInsecureContent(RenderFrameHostImpl* source,
                               const GURL& security_origin,
                               const GURL& target_url);
  void OnDidDisplayContentWithCertificateErrors(RenderFrameHostImpl* source);
  void OnDidRunContentWithCertificateErrors(RenderFrameHostImpl* source);
  void OnDidFinishLoad(RenderFrameHostImpl* source, const GURL& url);
  void OnGoToEntryAtOffset(RenderFrameHostImpl* source,
                           int offset,
                           bool has_user_gesture);
  void OnUpdateZoomLimits(RenderViewHostImpl* source,
                          int minimum_percent,
                          int maximum_percent);
  void OnPageScaleFactorChanged(RenderViewHostImpl* source,
                                float page_scale_factor);
  void OnTextAutosizerPageInfoChanged(
      RenderViewHostImpl* source,
      const blink::WebTextAutosizerPageInfo& page_info);

  void OnDomOperationResponse(RenderFrameHostImpl* source,
                              const std::string& json_string);
  void OnUpdatePageImportanceSignals(RenderFrameHostImpl* source,
                                     const PageImportanceSignals& signals);
#if BUILDFLAG(ENABLE_PLUGINS)
  void OnPepperInstanceCreated(RenderFrameHostImpl* source,
                               int32_t pp_instance);
  void OnPepperInstanceDeleted(RenderFrameHostImpl* source,
                               int32_t pp_instance);
  void OnPepperPluginHung(RenderFrameHostImpl* source,
                          int plugin_child_id,
                          const base::FilePath& path,
                          bool is_hung);
  void OnPepperStartsPlayback(RenderFrameHostImpl* source, int32_t pp_instance);
  void OnPepperStopsPlayback(RenderFrameHostImpl* source, int32_t pp_instance);
  void OnPluginCrashed(RenderFrameHostImpl* source,
                       const base::FilePath& plugin_path,
                       base::ProcessId plugin_pid);
  void OnRequestPpapiBrokerPermission(RenderViewHostImpl* source,
                                      int ppb_broker_route_id,
                                      const GURL& url,
                                      const base::FilePath& plugin_path);

  // Callback function when requesting permission to access the PPAPI broker.
  // |result| is true if permission was granted.
  void SendPpapiBrokerPermissionResult(int process_id,
                                       int ppb_broker_route_id,
                                       bool result);

  void OnBrowserPluginMessage(RenderFrameHost* render_frame_host,
                              const IPC::Message& message);
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  void OnUpdateFaviconURL(RenderFrameHostImpl* source,
                          const std::vector<FaviconURL>& candidates);
  void OnShowValidationMessage(RenderViewHostImpl* source,
                               const gfx::Rect& anchor_in_root_view,
                               const base::string16& main_text,
                               const base::string16& sub_text);
  void OnHideValidationMessage(RenderViewHostImpl* source);
  void OnMoveValidationMessage(RenderViewHostImpl* source,
                               const gfx::Rect& anchor_in_root_view);

  // Called by derived classes to indicate that we're no longer waiting for a
  // response. Will inform |delegate_| of the change in status so that it may,
  // for example, update the throbber.
  void SetNotWaitingForResponse();

  // Inner WebContents Helpers -------------------------------------------------
  //
  // These functions are helpers in managing a hierarchy of WebContents
  // involved in rendering inner WebContents.

  // Registers FrameSinkIds for all WebContents in the subtree of
  // WebContentsTree, rooted at |contents|.
  void RecursivelyRegisterFrameSinkIds();

  // When multiple WebContents are present within a tab or window, a single one
  // is focused and will route keyboard events in most cases to a RenderWidget
  // contained within it. |GetFocusedWebContents()|'s main frame widget will
  // receive page focus and blur events when the containing window changes focus
  // state.

  // Returns true if |this| is the focused WebContents or an ancestor of the
  // focused WebContents.
  bool ContainsOrIsFocusedWebContents();

  // Walks up the outer WebContents chain and focuses the FrameTreeNode where
  // each inner WebContents is attached.
  void FocusOuterAttachmentFrameChain();

  // Convenience method to notify observers that an inner WebContents was
  // created with |this| WebContents as its owner. This does *not* immediately
  // guarantee that |inner_web_contents| has been added to the WebContents tree.
  void InnerWebContentsCreated(WebContents* inner_web_contents);

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.

  // Helper for CreateNewWidget/CreateNewFullscreenWidget.
  void CreateNewWidget(int32_t render_process_id,
                       int32_t route_id,
                       bool is_fullscreen,
                       mojo::PendingRemote<mojom::Widget> widget,
                       RenderViewHostImpl* render_view_host);

  // Helper for ShowCreatedWidget/ShowCreatedFullscreenWidget.
  void ShowCreatedWidget(int process_id,
                         int route_id,
                         bool is_fullscreen,
                         const gfx::Rect& initial_rect);

  // Finds the new RenderWidgetHost and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  RenderWidgetHostView* GetCreatedWidget(int process_id, int route_id);

  // Finds the new WebContentsImpl by |main_frame_widget_route_id|, initializes
  // it for renderer-initiated creation, and returns it. Note that this can only
  // be called once as this call also removes it from the internal map.
  std::unique_ptr<WebContentsImpl> GetCreatedWindow(
      int process_id,
      int main_frame_widget_route_id);

  // Sends a Page message IPC.
  void SendPageMessage(IPC::Message* msg);

  void SetOpenerForNewContents(FrameTreeNode* opener, bool opener_suppressed);

  // Tracking loading progress -------------------------------------------------

  // Resets the tracking state of the current load progress.
  void ResetLoadProgressState();

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress();

  // Notifies the delegate of a change in loading state.
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable).
  // |due_to_interstitial| is true if the change in load state occurred because
  // an interstitial page started showing/proceeded.
  void LoadingStateChanged(bool to_different_document,
                           bool due_to_interstitial,
                           LoadNotificationDetails* details);

  // Misc non-view stuff -------------------------------------------------------

  // Sets the history for a specified RenderViewHost to |history_length|
  // entries, with an offset of |history_offset|.
  void SetHistoryOffsetAndLengthForView(RenderViewHost* render_view_host,
                                        int history_offset,
                                        int history_length);

  // Helper functions for sending notifications.
  void NotifyViewSwapped(RenderViewHost* old_host, RenderViewHost* new_host);
  void NotifyFrameSwapped(RenderFrameHost* old_host,
                          RenderFrameHost* new_host,
                          bool is_main_frame);
  void NotifyDisconnected();

  // TODO(creis): This should take in a FrameTreeNode to know which node's
  // render manager to return.  For now, we just return the root's.
  RenderFrameHostManager* GetRenderManager() const;

  // Removes browser plugin embedder if there is one.
  void RemoveBrowserPluginEmbedder();

  // Helper function to invoke WebContentsDelegate::GetSizeForNewRenderView().
  gfx::Size GetSizeForNewRenderView(bool is_main_frame);

  void OnFrameRemoved(RenderFrameHost* render_frame_host);

  // Helper method that's called whenever |preferred_size_| or
  // |preferred_size_for_capture_| changes, to propagate the new value to the
  // |delegate_|.
  void OnPreferredSizeChanged(const gfx::Size& old_size);

  // Internal helper to create WebUI objects associated with |this|. |url| is
  // used to determine which WebUI should be created (if any).
  std::unique_ptr<WebUIImpl> CreateWebUI(const GURL& url);

  void SetJavaScriptDialogManagerForTesting(
      JavaScriptDialogManager* dialog_manager);

  // Returns the FindRequestManager, which may be found in an outer WebContents.
  FindRequestManager* GetFindRequestManager();

  // Returns the FindRequestManager, or tries to create one if it doesn't
  //  already exist. The FindRequestManager may be found in an outer
  // WebContents. If this is an inner WebContents which is not yet attached to
  // an outer WebContents the method will return nullptr.
  FindRequestManager* GetOrCreateFindRequestManager();

  // Removes a registered WebContentsBindingSet by interface name.
  void RemoveBindingSet(const std::string& interface_name);

  // Prints a console warning when visiting a localhost site with a bad
  // certificate via --allow-insecure-localhost.
  void ShowInsecureLocalhostWarningIfNeeded();

  // Format of |headers| is a new line separated list of key value pairs:
  // "<key1>: <value1>\r\n<key2>: <value2>".
  static download::DownloadUrlParameters::RequestHeadersType
  ParseDownloadHeaders(const std::string& headers);

  // Sets the visibility of immediate child views, i.e. views whose parent view
  // is that of the main frame.
  void SetVisibilityForChildViews(bool visible);

  // A helper for clearing the link status bubble after navigating away.
  // See also UpdateTargetURL.
  void ClearTargetURL();

  class AXTreeSnapshotCombiner;
  void RecursiveRequestAXTreeSnapshotOnFrame(FrameTreeNode* root_node,
                                             AXTreeSnapshotCombiner* combiner,
                                             ui::AXMode ax_mode);

  // Called each time |fullscreen_frames_| is updated. Find the new
  // |current_fullscreen_frame_| and notify observers whenever it changes.
  void FullscreenFrameSetUpdated();

  // Sets the visibility to |new_visibility| and propagates this to the
  // renderer side, taking into account the current capture state. This
  // can be called with the current visibility to effect capturing
  // changes.
  void UpdateVisibilityAndNotifyPageAndView(Visibility new_visibility);

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  WebContentsDelegate* delegate_;

  // Handles the back/forward list and loading.
  NavigationControllerImpl controller_;

  // The corresponding view.
  std::unique_ptr<WebContentsView> view_;

  // The view of the RVHD. Usually this is our WebContentsView implementation,
  // but if an embedder uses a different WebContentsView, they'll need to
  // provide this.
  RenderViewHostDelegateView* render_view_host_delegate_view_;

  // Tracks created WebContentsImpl objects that have not been shown yet. They
  // are identified by the process ID and routing ID passed to CreateNewWindow.
  std::map<GlobalRoutingID, std::unique_ptr<WebContentsImpl>> pending_contents_;

  // This map holds widgets that were created on behalf of the renderer but
  // haven't been shown yet.
  std::map<GlobalRoutingID, RenderWidgetHostView*> pending_widget_views_;

  std::map<WebContentsImpl*, std::unique_ptr<DestructionObserver>>
      destruction_observers_;

  // A list of observers notified when page state changes. Weak references.
  // This MUST be listed above frame_tree_ since at destruction time the
  // latter might cause RenderViewHost's destructor to call us and we might use
  // the observer list then.
  base::ObserverList<WebContentsObserver>::Unchecked observers_;

  // Associated interface binding sets attached to this WebContents.
  std::map<std::string, WebContentsBindingSet*> binding_sets_;

  // True if this tab was opened by another tab. This is not unset if the opener
  // is closed.
  bool created_with_opener_;

#if defined(OS_ANDROID)
  std::unique_ptr<WebContentsAndroid> web_contents_android_;
#endif

  // Helper classes ------------------------------------------------------------

  // Contains information about the WebContents tree structure.
  WebContentsTreeNode node_;

  // Manages the frame tree of the page and process swaps in each node.
  FrameTree frame_tree_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Manages/coordinates multi-process find-in-page requests. Created lazily.
  std::unique_ptr<FindRequestManager> find_request_manager_;

  // Data for loading state ----------------------------------------------------

  // Indicates whether the current load is to a different document. Only valid
  // if |is_loading_| is true and only tracks loads in the main frame.
  // TODO(pbos): Check navigation requests and handles instead of caching this.
  bool is_load_to_different_document_;

  // Indicates if the tab is considered crashed.
  base::TerminationStatus crashed_status_;
  int crashed_error_code_;

  // Whether this WebContents is waiting for a first-response for the
  // main resource of the page. This controls whether the throbber state is
  // "waiting" or "loading."
  bool waiting_for_response_;

  // The current load state and the URL associated with it.
  net::LoadStateWithParam load_state_;
  base::string16 load_state_host_;

  base::TimeTicks loading_last_progress_update_;

  // Upload progress, for displaying in the status bar.
  // Set to zero when there is no significant upload happening.
  uint64_t upload_size_;
  uint64_t upload_position_;

  // Tracks that this WebContents needs to unblock requests to the renderer.
  // See ResumeLoadingCreatedWebContents.
  bool is_resume_pending_;

  // The interstitial page currently shown, if any. Not owned by this class: the
  // InterstitialPage is self-owned and deletes itself asynchronously when
  // hidden. Because it may outlive this WebContents, it enters a disabled state
  // when hidden or preparing for destruction.
  InterstitialPageImpl* interstitial_page_;

  // Data for current page -----------------------------------------------------

  // When a title cannot be taken from any entry, this title will be used.
  base::string16 page_title_when_no_navigation_entry_;

  // When a navigation occurs, we record its contents MIME type. It can be
  // used to check whether we can do something for some special contents.
  std::string contents_mime_type_;

  // Whether the initial empty page has been accessed by another page, making it
  // unsafe to show the pending URL. Usually false unless another window tries
  // to modify the blank page.  Always false after the first commit.
  bool has_accessed_initial_document_;

  // The last published theme color.
  base::Optional<SkColor> last_sent_theme_color_;

  // SourceId for current page from the last committed cross-document
  // navigation.
  ukm::SourceId last_committed_source_id_ = ukm::kInvalidSourceId;
  // SourceId of the last committed navigation, either a cross-document or
  // same-document navigation.
  ukm::SourceId last_committed_source_id_including_same_document_ =
      ukm::kInvalidSourceId;

  // Data for misc internal state ----------------------------------------------

  // When either > 0, the WebContents is currently being captured (e.g.,
  // for screenshots or mirroring); and the underlying RenderWidgetHost
  // should not be told it is hidden. If |visible_capturer_count_| > 0,
  // the underlying Page is set to fully visible. Otherwise, it is set
  // to be hidden but still paint.
  int visible_capturer_count_;
  int hidden_capturer_count_;

  // The visibility of the WebContents. Initialized from
  // |CreateParams::initially_hidden|. Updated from
  // UpdateWebContentsVisibility(), WasShown(), WasHidden(), WasOccluded().
  Visibility visibility_ = Visibility::VISIBLE;

  // Whether there has been a call to UpdateWebContentsVisibility(VISIBLE).
  bool did_first_set_visible_ = false;

  // See getter above.
  bool is_being_destroyed_;

  // Keep track of whether this WebContents is currently iterating over its list
  // of observers, during which time it should not be deleted.
  bool is_notifying_observers_;

  // Indicates whether we should notify about disconnection of this
  // WebContentsImpl. This is used to ensure disconnection notifications only
  // happen if a connection notification has happened and that they happen only
  // once.
  bool notify_disconnection_;

  // Set to true if we shouldn't send input events.
  bool ignore_input_events_ = false;

  // Pointer to the JavaScript dialog manager, lazily assigned. Used because the
  // delegate of this WebContentsImpl is nulled before its destructor is called.
  JavaScriptDialogManager* dialog_manager_;

  // Set to true when there is an active JavaScript dialog showing.
  bool is_showing_javascript_dialog_ = false;

  // Set to true when there is an active "before unload" dialog.  When true,
  // we've forced the throbber to start in Navigate, and we need to remember to
  // turn it off in OnJavaScriptMessageBoxClosed if the navigation is canceled.
  bool is_showing_before_unload_dialog_;

  // Settings that get passed to the renderer process.
  blink::mojom::RendererPreferences renderer_preferences_;

  // The time that this WebContents was last made active. The initial value is
  // the WebContents creation time.
  base::TimeTicks last_active_time_;

  // The time that this WebContents last received an 'interactive' input event
  // from the user. Interactive input events are things like mouse clicks and
  // keyboard input, but not mouse wheel scrolling or mouse moves.
  base::TimeTicks last_interactive_input_event_time_;

  // See description above setter.
  bool closed_by_user_gesture_;

  // Minimum/maximum zoom percent.
  const int minimum_zoom_percent_;
  const int maximum_zoom_percent_;

  // Used to correctly handle integer zooming through a smooth scroll device.
  float zoom_scroll_remainder_;

  // The intrinsic size of the page.
  gfx::Size preferred_size_;

  // The preferred size for content screen capture.  When |capturer_count_| > 0,
  // this overrides |preferred_size_|.
  gfx::Size preferred_size_for_capture_;

  // Size set by a top-level frame with auto-resize enabled. This is needed by
  // out-of-process iframes for their visible viewport size.
  gfx::Size auto_resize_size_;

  // When device emulation is enabled, override the size of current and newly
  // created render views/widgets.
  gfx::Size device_emulation_size_;
  gfx::Size view_size_before_emulation_;

  // Holds information about a current color chooser dialog, if one is visible.
  class ColorChooser;
  std::unique_ptr<ColorChooser> color_chooser_;

  // Manages the embedder state for browser plugins, if this WebContents is an
  // embedder; NULL otherwise.
  std::unique_ptr<BrowserPluginEmbedder> browser_plugin_embedder_;
  // Manages the guest state for browser plugin, if this WebContents is a guest;
  // NULL otherwise.
  std::unique_ptr<BrowserPluginGuest> browser_plugin_guest_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Manages the whitelist of plugin content origins exempt from power saving.
  std::unique_ptr<PluginContentOriginWhitelist>
      plugin_content_origin_whitelist_;
#endif

  // This must be at the end, or else we might get notifications and use other
  // member variables that are gone.
  NotificationRegistrar registrar_;

  // All live RenderWidgetHostImpls that are created by this object and may
  // outlive it.
  std::set<RenderWidgetHostImpl*> created_widgets_;

  // Process id of the shown fullscreen widget, or kInvalidUniqueID if there is
  // no fullscreen widget.
  int fullscreen_widget_process_id_;

  // Routing id of the shown fullscreen widget or MSG_ROUTING_NONE otherwise.
  int fullscreen_widget_routing_id_;

  // At the time the fullscreen widget was being shut down, did it have focus?
  // This is used to restore focus to the WebContentsView after both: 1) the
  // fullscreen widget is destroyed, and 2) the WebContentsDelegate has
  // completed making layout changes to effect an exit from fullscreen mode.
  bool fullscreen_widget_had_focus_at_shutdown_;

  // When a new tab is created asynchronously, stores the OpenURLParams needed
  // to continue loading the page once the tab is ready.
  std::unique_ptr<OpenURLParams> delayed_open_url_params_;

  // When a new tab is created with window.open(), navigation can be deferred
  // to execute asynchronously. In such case, the parameters need to be saved
  // for the navigation to be started at a later point.
  std::unique_ptr<NavigationController::LoadURLParams> delayed_load_url_params_;

  // Whether overscroll should be unconditionally disabled.
  bool force_disable_overscroll_content_;

  // Whether the last JavaScript dialog shown was suppressed. Used for testing.
  bool last_dialog_suppressed_;

  mojo::Remote<device::mojom::GeolocationContext> geolocation_context_;

  std::unique_ptr<WakeLockContextHost> wake_lock_context_host_;

  mojo::ReceiverSet<blink::mojom::ColorChooserFactory>
      color_chooser_factory_receivers_;

  std::unique_ptr<ScreenOrientationProvider> screen_orientation_provider_;

  std::unique_ptr<ManifestManagerHost> manifest_manager_host_;

  // The accessibility mode for all frames. This is queried when each frame
  // is created, and broadcast to all frames when it changes.
  ui::AXMode accessibility_mode_;

  std::unique_ptr<content::AccessibilityEventRecorder> event_recorder_;

  // Monitors power levels for audio streams associated with this WebContents.
  AudioStreamMonitor audio_stream_monitor_;

  // Coordinates all the audio streams for this WebContents. Lazily initialized.
  base::Optional<ForwardingAudioStreamFactory> audio_stream_factory_;

  // Created on-demand to mute all audio output from this WebContents.
  std::unique_ptr<WebContentsAudioMuter> audio_muter_;

  size_t bluetooth_connected_device_count_ = 0;
  size_t serial_active_frame_count_ = 0;
  size_t hid_active_frame_count_ = 0;

  size_t native_file_system_handle_count_ = 0;
  std::map<base::FilePath, size_t> native_file_system_directory_handles_;
  size_t native_file_system_writable_handle_count_ = 0;

  bool has_picture_in_picture_video_ = false;

  // Manages media players, CDMs, and power save blockers for media.
  std::unique_ptr<MediaWebContentsObserver> media_web_contents_observer_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Observes pepper playback changes, and notifies MediaSession.
  std::unique_ptr<PepperPlaybackObserver> pepper_playback_observer_;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  std::unique_ptr<RenderWidgetHostInputEventRouter> rwh_input_event_router_;

  PageImportanceSignals page_importance_signals_;

#if !defined(OS_ANDROID)
  bool page_scale_factor_is_one_;
#endif  // !defined(OS_ANDROID)

  // TextInputManager tracks the IME-related state for all the
  // RenderWidgetHostViews on this WebContents. Only exists on the outermost
  // WebContents and is automatically destroyed when a WebContents becomes an
  // inner WebContents by attaching to an outer WebContents. Then the
  // IME-related state for RenderWidgetHosts on the inner WebContents is tracked
  // by the TextInputManager in the outer WebContents.
  std::unique_ptr<TextInputManager> text_input_manager_;

  // Stores the RenderWidgetHost that currently holds a mouse lock or nullptr if
  // there's no RenderWidgetHost holding a lock.
  RenderWidgetHostImpl* mouse_lock_widget_ = nullptr;

  // Stores the RenderWidgetHost that currently holds a keyboard lock or nullptr
  // if no RenderWidgetHost has the keyboard locked.
  RenderWidgetHostImpl* keyboard_lock_widget_ = nullptr;

  // Indicates whether the escape key is one of the requested keys to be locked.
  // This information is used to drive the browser UI so the correct exit
  // instructions are displayed to the user in fullscreen mode.
  bool esc_key_locked_ = false;

#if defined(OS_ANDROID)
  std::unique_ptr<service_manager::InterfaceProvider> java_interfaces_;
#endif

  // Whether this WebContents is for content overlay.
  bool is_overlay_content_;

  bool showing_context_menu_;

  int currently_playing_video_count_ = 0;
  base::flat_map<MediaPlayerId, gfx::Size> cached_video_sizes_;

  bool has_persistent_video_ = false;

  bool is_spatial_navigation_disabled_ = false;

  bool is_currently_audible_ = false;
  bool was_ever_audible_ = false;

  // Helper variable for resolving races in UpdateTargetURL / ClearTargetURL.
  RenderViewHost* view_that_set_last_target_url_ = nullptr;

  // Whether we should override user agent in new tabs.
  bool should_override_user_agent_in_new_tabs_ = false;

  // Gets notified about changes in viewport fit events.
  std::unique_ptr<DisplayCutoutHostImpl> display_cutout_host_impl_;

  // Stores a set of frames that are fullscreen.
  // See https://fullscreen.spec.whatwg.org.
  std::set<RenderFrameHostImpl*> fullscreen_frames_;

  // Store the frame that is currently fullscreen, nullptr if there is none.
  RenderFrameHostImpl* current_fullscreen_frame_ = nullptr;

  // Whether location bar should be focused by default. This is computed in
  // DidStartNavigation/DidFinishNavigation and only set for an initial
  // navigation triggered by the browser going to about:blank.
  bool should_focus_location_bar_by_default_ = false;

  // Stores the Portal object associated with this WebContents, if there is one.
  // If non-null then this WebContents is embedded in a portal and its outer
  // WebContents can be found by using GetOuterWebContents().
  Portal* portal_ = nullptr;

  // Stores information from the main frame's renderer that needs to be shared
  // with OOPIF renderers.
  blink::WebTextAutosizerPageInfo text_autosizer_page_info_;

  // TODO(crbug.com/934637): Remove this field when pdf/any inner web contents
  // user gesture is properly propagated. This is a temporary fix for history
  // intervention to be disabled for pdfs (crbug.com/965434).
  bool had_inner_webcontents_;

  // Prevents navigations in this contents while a javascript modal dialog is
  // showing.
  std::unique_ptr<JavaScriptDialogNavigationDeferrer>
      javascript_dialog_navigation_deferrer_;

  // The max number of loaded frames that have been seen in this WebContents.
  // This number is reset with each main frame navigation.
  size_t max_loaded_frame_count_ = 0;

  // This boolean value is used to keep track of whether we finished the first
  // successful navigation in this WebContents.
  bool first_navigation_completed_ = false;

  base::WeakPtrFactory<WebContentsImpl> loading_weak_factory_{this};
  base::WeakPtrFactory<WebContentsImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebContentsImpl);
};

// Dangerous methods which should never be made part of the public API, so we
// grant their use only to an explicit friend list (c++ attorney/client idiom).
class CONTENT_EXPORT WebContentsImpl::FriendWrapper {
 public:
  using CreatedCallback = base::RepeatingCallback<void(WebContents*)>;

 private:
  friend class TestNavigationObserver;
  friend class WebContentsAddedObserver;
  friend class ContentBrowserSanityChecker;

  FriendWrapper();  // Not instantiable.

  // Adds/removes a callback called on creation of each new WebContents.
  static void AddCreatedCallbackForTesting(const CreatedCallback& callback);
  static void RemoveCreatedCallbackForTesting(const CreatedCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(FriendWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
