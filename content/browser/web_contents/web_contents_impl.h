// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_handle_impl.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_delegate.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/page_delegate.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/web_contents/file_chooser_impl.h"
#include "content/browser/web_contents/slow_web_preference_cache.h"
#include "content/common/content_export.h"
#include "content/public/browser/fullscreen_types.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/mhtml_generation_result.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_states.h"
#include "net/base/network_handle.h"
#include "partition_alloc/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/text_autosizer_page_info.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom-shared.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace base {
class FilePath;
}  // namespace base

namespace device {
namespace mojom {
class WakeLock;
}
}  // namespace device

namespace input {
class RenderWidgetHostInputEventRouter;
}  // namespace input

namespace network::mojom {
class SharedDictionaryAccessDetails;
}  // namespace network::mojom

namespace service_manager {
class InterfaceProvider;
}  // namespace service_manager

namespace ui {
struct AXUpdatesAndEvents;
}

namespace content {
class JavaScriptDialogDismissNotifier;
enum class PictureInPictureResult;
class BeforeUnloadBlockingDelegate;  // content_browser_test_utils_internal.h
class BrowserPluginEmbedder;
class BrowserPluginGuest;
class FindRequestManager;
class JavaScriptDialogManager;
class MediaSession;
class MediaWebContentsObserver;
class NFCHost;
class RenderFrameHost;
class RenderFrameHostImpl;
class RenderViewHost;
class RenderViewHostDelegateView;
class RenderWidgetHostImpl;
class SafeAreaInsetsHost;
class SavePackage;
class ScopedAccessibilityMode;
class ScreenChangeMonitor;
class ScreenOrientationProvider;
class SiteInstanceGroup;
// For web_contents_impl_browsertest.cc
class TestWCDelegateForDialogsAndFullscreen;
class TestWebContents;
class TextInputManager;
class TouchEmulatorImpl;
class WakeLockContextHost;
class WebContentsDelegate;
class WebContentsImpl;
class WebContentsView;
struct MHTMLGenerationParams;
class PreloadingAttempt;

namespace mojom {
class CreateNewWindowParams;
}  // namespace mojom

#if BUILDFLAG(IS_ANDROID)
class WebContentsAndroid;
#endif

#if BUILDFLAG(ENABLE_PPAPI)
class PepperPlaybackObserver;
#endif

// CreatedWindow holds the WebContentsImpl and target url between IPC calls to
// CreateNewWindow and ShowCreatedWindow.
struct CONTENT_EXPORT CreatedWindow {
  CreatedWindow();
  CreatedWindow(std::unique_ptr<WebContentsImpl> contents, GURL target_url);
  CreatedWindow(CreatedWindow&&);
  CreatedWindow(const CreatedWindow&) = delete;
  CreatedWindow& operator=(CreatedWindow&&);
  CreatedWindow& operator=(const CreatedWindow&) = delete;
  ~CreatedWindow();

  std::unique_ptr<WebContentsImpl> contents;
  GURL target_url;
};

using PageVisibilityState = blink::mojom::PageVisibilityState;

using ClipboardPasteData = content::ClipboardPasteData;

class CONTENT_EXPORT WebContentsImpl
    : public WebContents,
      public FrameTree::Delegate,
      public RenderFrameHostDelegate,
      public RenderViewHostDelegate,
      public RenderWidgetHostDelegate,
      public RenderFrameHostManager::Delegate,
      public PageDelegate,
      public blink::mojom::ColorChooserFactory,
      public NavigationControllerDelegate,
      public NavigatorDelegate,
      public ui::NativeThemeObserver,
      public ui::ColorProviderSourceObserver,
      public SlowWebPreferenceCacheObserver,
      public input::RenderWidgetHostInputEventRouter::Delegate {
 public:
  class FriendWrapper;

  WebContentsImpl(const WebContentsImpl&) = delete;
  WebContentsImpl& operator=(const WebContentsImpl&) = delete;

  ~WebContentsImpl() override;

  static std::unique_ptr<WebContentsImpl> CreateWithOpener(
      const WebContents::CreateParams& params,
      RenderFrameHostImpl* opener_rfh);

  static std::vector<WebContentsImpl*> GetAllWebContents();

  static WebContentsImpl* FromFrameTreeNode(
      const FrameTreeNode* frame_tree_node);
  static WebContents* FromRenderFrameHostID(
      GlobalRenderFrameHostId render_frame_host_id);
  static WebContents* FromRenderFrameHostID(int render_process_host_id,
                                            int render_frame_host_id);
  static WebContents* FromFrameTreeNodeId(FrameTreeNodeId frame_tree_node_id);
  static WebContentsImpl* FromOuterFrameTreeNode(
      const FrameTreeNode* frame_tree_node);
  static WebContentsImpl* FromRenderWidgetHostImpl(RenderWidgetHostImpl* rwh);
  static WebContentsImpl* FromRenderFrameHostImpl(RenderFrameHostImpl* rfh);

  // Complex initialization here. Specifically needed to avoid having
  // members call back into our virtual functions in the constructor.
  // The primary main frame policy might be passed down as it is inherited from
  // the opener when WebContents is created with an opener.
  virtual void Init(const WebContents::CreateParams& params,
                    blink::FramePolicy primary_main_frame_policy);

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

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
                         ui::mojom::DragOperation operation,
                         RenderWidgetHost* source_rwh);

  // Notification that the RenderViewHost's load state changed.
  void LoadStateChanged(network::mojom::LoadInfoPtr load_info);

  // Updates the visibility and notifies observers. Note that this is
  // distinct from UpdateWebContentsVisibility which may also update the
  // visibility of renderer-side objects.
  void SetVisibilityAndNotifyObservers(Visibility visibility);

  // Notify observers that the web contents has been focused.
  void NotifyWebContentsFocused(RenderWidgetHost* render_widget_host);

  // Notify observers that the web contents has lost focus.
  void NotifyWebContentsLostFocus(RenderWidgetHost* render_widget_host);

  WebContentsView* GetView() const;

  // Called on screen information changes; |is_multi_screen_changed| is true iff
  // the plurality of connected screens changed (e.g. 1 screen <-> 2 screens).
  void OnScreensChange(bool is_multi_screen_changed);

  void OnScreenOrientationChange();

  ScreenOrientationProvider* GetScreenOrientationProviderForTesting() const {
    return screen_orientation_provider_.get();
  }

  // Return RenderWidgetHostView from RWHIER.
  std::vector<RenderWidgetHostView*> GetRenderWidgetHostViewsForTests();

  bool IsDelegatedInkRendererBoundForTest() {
    return delegated_ink_point_renderer_.is_bound();
  }

  // Adds the given accessibility mode to the current accessibility mode
  // bitmap.
  void AddAccessibilityModeForTesting(ui::AXMode mode);

  // Sets the zoom level for frames associated with this WebContents.
  void UpdateZoom();

  // Sets the zoom level for frames associated with this WebContents if it
  // matches |host| and (if non-empty) |scheme|. Matching is done on the
  // last committed entry.
  void UpdateZoomIfNecessary(const std::string& scheme,
                             const std::string& host);

  // Returns the focused WebContents.
  // If there are multiple inner/outer WebContents (when embedding <webview>,
  // <guestview>, ...) returns the single one containing the currently focused
  // frame. Otherwise, returns this WebContents.
  WebContentsImpl* GetFocusedWebContents();

  // Returns the focused FrameTree. For MPArch we may return a different
  // focused frame tree even though the focused WebContents is the same.
  FrameTree* GetFocusedFrameTree();

  // TODO(lukasza): Maybe this method can be removed altogether (so that the
  // focus of the location bar is only set in the //chrome layer).
  void SetFocusToLocationBar();

  // Returns a vector containing this WebContents and all inner WebContents
  // within it (recursively).
  std::vector<WebContentsImpl*> GetWebContentsAndAllInner();

  // Returns the primary FrameTree for this WebContents (as opposed to the
  // ones held by MPArch features like Prerender or Fenced Frame).
  // See docs/frame_trees.md for more details.
  FrameTree& GetPrimaryFrameTree() { return primary_frame_tree_; }

  // Whether the initial empty page of this view has been accessed by another
  // page, making it unsafe to show the pending URL. Always false after the
  // first commit.
  // TODO(crbug.com/40165695): Rename to HasAccessedInitialMainDocument
  bool HasAccessedInitialDocument();

#if BUILDFLAG(IS_ANDROID)
  void SetPrimaryMainFrameImportance(ChildProcessImportance importance);
#endif

  // Returns the human-readable name for title in Media Controls.
  // If the returned value is an empty string, it means that there is no
  // human-readable name.
  std::string GetTitleForMediaControls();

  // Sets the accessibility mode if this WebContents will potentially be
  // user-visible, and broadcasts it to all of its frames if it differs from the
  // previous mode.
  void SetAccessibilityMode(ui::AXMode mode);

  // Inform the WebContentsImpl object that a write-access Captured Surface
  // Control API was invoked (sendWheel, setZoomLevel) for this object.
  // A notification is then propagated to observers.
  void DidCapturedSurfaceControl();

  // Let long press on links select the link text instead of triggering
  // the context menu.
#if BUILDFLAG(IS_ANDROID)
  void SetLongPressLinkSelectText(bool enabled);
#endif

  // WebContents ------------------------------------------------------
  WebContentsDelegate* GetDelegate() final;
  void SetDelegate(WebContentsDelegate* delegate) override;
  NavigationControllerImpl& GetController() override;
  BrowserContext* GetBrowserContext() override;
  base::WeakPtr<WebContents> GetWeakPtr() override;
  const GURL& GetURL() override;
  const GURL& GetVisibleURL() override;
  const GURL& GetLastCommittedURL() override;
  const RenderFrameHostImpl* GetPrimaryMainFrame() const override;
  RenderFrameHostImpl* GetPrimaryMainFrame() override;
  PageImpl& GetPrimaryPage() override;
  RenderFrameHostImpl* GetFocusedFrame() override;
  bool IsPrerenderedFrame(FrameTreeNodeId frame_tree_node_id) override;
  RenderFrameHostImpl* UnsafeFindFrameByFrameTreeNodeId(
      FrameTreeNodeId frame_tree_node_id) override;
  void ForEachRenderFrameHostWithAction(
      base::FunctionRef<FrameIterationAction(RenderFrameHost*)> on_frame)
      override;
  void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHost*)> on_frame) override;
  RenderViewHostImpl* GetRenderViewHost() override;
  RenderWidgetHostView* GetRenderWidgetHostView() override;
  RenderWidgetHostView* GetTopLevelRenderWidgetHostView() override;
  void ClosePage() override;
  std::optional<SkColor> GetThemeColor() override;
  std::optional<SkColor> GetBackgroundColor() override;
  void SetPageBaseBackgroundColor(std::optional<SkColor> color) override;
  void SetColorProviderSource(ui::ColorProviderSource* source) override;
  ui::ColorProviderKey::ColorMode GetColorMode() const override;
  WebUI* GetWebUI() override;
  void SetUserAgentOverride(const blink::UserAgentOverride& ua_override,
                            bool override_in_new_tabs) override;
  void SetRendererInitiatedUserAgentOverrideOption(
      NavigationController::UserAgentOverrideOption option) override;
  const blink::UserAgentOverride& GetUserAgentOverride() override;
  bool ShouldOverrideUserAgentForRendererInitiatedNavigation() override;
  void SetAlwaysSendSubresourceNotifications() override;
  bool GetSendSubresourceNotification() override;
  bool IsWebContentsOnlyAccessibilityModeForTesting() override;
  bool IsFullAccessibilityModeForTesting() override;
  const std::u16string& GetTitle() override;
  const std::optional<std::u16string>& GetAppTitle() override;
  void UpdateTitleForEntry(NavigationEntry* entry,
                           const std::u16string& title) override;
  SiteInstanceImpl* GetSiteInstance() override;
  bool IsLoading() override;
  double GetLoadProgress() override;
  bool ShouldShowLoadingUI() override;
  bool IsDocumentOnLoadCompletedInPrimaryMainFrame() override;
  bool IsWaitingForResponse() override;
  bool HasUncommittedNavigationInPrimaryMainFrame() override;
  const net::LoadStateWithParam& GetLoadState() override;
  const std::u16string& GetLoadStateHost() override;
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                             ui::AXMode ax_mode,
                             size_t max_nodes,
                             base::TimeDelta timeout,
                             AXTreeSnapshotPolicy policy) override;
  uint64_t GetUploadSize() override;
  uint64_t GetUploadPosition() override;
  const std::string& GetEncoding() override;
  void Discard() override;
  bool WasDiscarded() override;
  void SetWasDiscarded(bool was_discarded) override;
  [[nodiscard]] base::ScopedClosureRunner IncrementCapturerCount(
      const gfx::Size& capture_size,
      bool stay_hidden,
      bool stay_awake,
      bool is_activity) override;
  const blink::mojom::CaptureHandleConfig& GetCaptureHandleConfig() override;
  bool IsBeingCaptured() override;
  bool IsBeingVisiblyCaptured() override;
  bool IsAudioMuted() override;
  void SetAudioMuted(bool mute) override;
  bool IsCurrentlyAudible() override;
  bool IsConnectedToBluetoothDevice() override;
  bool IsScanningForBluetoothDevices() override;
  bool IsConnectedToSerialPort() override;
  bool IsConnectedToHidDevice() override;
  bool IsConnectedToUsbDevice() override;
  bool HasFileSystemAccessHandles() override;
  bool HasPictureInPictureVideo() override;
  bool HasPictureInPictureDocument() override;
  bool IsCrashed() override;
  base::TerminationStatus GetCrashedStatus() override;
  int GetCrashedErrorCode() override;
  bool IsBeingDestroyed() override;
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override;
  void OnAudioStateChanged() override;
  base::TimeTicks GetLastActiveTimeTicks() override;
  base::Time GetLastActiveTime() override;
  void WasShown() override;
  void WasHidden() override;
  void WasOccluded() override;
  Visibility GetVisibility() override;
  bool NeedToFireBeforeUnloadOrUnloadEvents() override;
  void DispatchBeforeUnload(bool auto_cancel) override;
  void AttachInnerWebContents(
      std::unique_ptr<WebContents> inner_web_contents,
      RenderFrameHost* render_frame_host,
      bool is_full_page) override;
  bool IsInnerWebContentsForGuest() override;
  RenderFrameHostImpl* GetOuterWebContentsFrame() override;
  WebContentsImpl* GetOuterWebContents() override;
  WebContentsImpl* GetOutermostWebContents() override;
  std::vector<WebContents*> GetInnerWebContents() override;
  WebContentsImpl* GetResponsibleWebContents() override;
  void DidChangeVisibleSecurityState() override;
  void SyncRendererPrefs() override;
  void Stop() override;
  void SetPageFrozen(bool frozen) override;
  std::unique_ptr<WebContents> Clone() override;
  void ReloadFocusedFrame() override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void CopyToFindPboard() override;
  void CenterSelection() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void Delete() override;
  void SelectAll() override;
  void CollapseSelection() override;
  void ScrollToTopOfDocument() override;
  void ScrollToBottomOfDocument() override;
  void Replace(const std::u16string& word) override;
  void ReplaceMisspelling(const std::u16string& word) override;
  void NotifyContextMenuClosed(const GURL& link_followed) override;
  void ExecuteCustomContextMenuCommand(int action,
                                       const GURL& link_followed) override;
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
  bool IsSavable() override;
  void OnSavePage() override;
  bool SavePage(const base::FilePath& main_file,
                const base::FilePath& dir_path,
                SavePageType save_type) override;
  void SaveFrame(const GURL& url,
                 const Referrer& referrer,
                 RenderFrameHost* rfh) override;
  void SaveFrameWithHeaders(const GURL& url,
                            const Referrer& referrer,
                            const std::string& headers,
                            const std::u16string& suggested_filename,
                            RenderFrameHost* rfh,
                            bool is_subresource) override;
  void GenerateMHTML(const MHTMLGenerationParams& params,
                     base::OnceCallback<void(int64_t)> callback) override;
  void GenerateMHTMLWithResult(
      const MHTMLGenerationParams& params,
      MHTMLGenerationResult::GenerateMHTMLCallback callback) override;
  const std::string& GetContentsMimeType() override;
  blink::RendererPreferences* GetMutableRendererPrefs() override;
  void Close() override;
  void SetClosedByUserGesture(bool value) override;
  bool GetClosedByUserGesture() override;
  int GetMinimumZoomPercent() override;
  int GetMaximumZoomPercent() override;
  void SetPageScale(float page_scale_factor) override;
  gfx::Size GetPreferredSize() override;
  bool GotResponseToPointerLockRequest(
      blink::mojom::PointerLockResult result) override;
  void GotPointerLockPermissionResponse(bool allowed) override;
  void DropPointerLockForTesting() override;
  bool GotResponseToKeyboardLockRequest(bool allowed) override;
  bool HasOpener() override;
  RenderFrameHostImpl* GetOpener() override;
  bool HasLiveOriginalOpenerChain() override;
  WebContents* GetFirstWebContentsInLiveOriginalOpenerChain() override;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void DidChooseColorInColorChooser(SkColor color) override;
  void DidEndColorChooser() override;
#endif
  int DownloadImageFromAxNode(const ui::AXTreeID tree_id,
                              const ui::AXNodeID node_id,
                              const gfx::Size& preferred_size,
                              uint32_t max_bitmap_size,
                              bool bypass_cache,
                              ImageDownloadCallback callback) override;
  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    const gfx::Size& preferred_size,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    ImageDownloadCallback callback) override;

  int DownloadImageInFrame(
      const GlobalRenderFrameHostId& initiator_frame_routing_id,
      const GURL& url,
      bool is_favicon,
      const gfx::Size& preferred_size,
      uint32_t max_bitmap_size,
      bool bypass_cache,
      WebContents::ImageDownloadCallback callback) override;
  void Find(int request_id,
            const std::u16string& search_text,
            blink::mojom::FindOptionsPtr options,
            bool skip_delay) override;
  void StopFinding(StopFindAction action) override;
  bool WasEverAudible() override;
  bool IsFullscreen() override;
  bool ShouldShowStaleContentOnEviction() override;
  void ExitFullscreen(bool will_cause_resize) override;
  [[nodiscard]] base::ScopedClosureRunner ForSecurityDropFullscreen(
      int64_t display_id) override;
  void ResumeLoadingCreatedWebContents() override;
  void SetIsOverlayContent(bool is_overlay_content) override;
  bool IsFocusedElementEditable() override;
  void ClearFocusedElement() override;
  bool IsShowingContextMenu() override;
  void SetShowingContextMenu(bool showing) override;
  base::UnguessableToken GetAudioGroupId() override;
  bool CompletedFirstVisuallyNonEmptyPaint() override;
  void UpdateFaviconURL(
      RenderFrameHostImpl* source,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  const std::vector<blink::mojom::FaviconURLPtr>& GetFaviconURLs() override;
  void Resize(const gfx::Rect& new_bounds) override;
  gfx::Size GetSize() override;
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) override;
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents() override;
  base::android::ScopedJavaLocalRef<jthrowable> GetJavaCreatorLocation()
      override;
  WebContentsAndroid* GetWebContentsAndroid();
  void ClearWebContentsAndroid();
  void ActivateNearestFindResult(float x, float y) override;
  void RequestFindMatchRects(int current_version) override;
  service_manager::InterfaceProvider* GetJavaInterfaces() override;
#endif
  bool HasRecentInteraction() override;
  [[nodiscard]] ScopedIgnoreInputEvents IgnoreInputEvents(
      std::optional<WebInputEventAuditCallback> audit_callback) override;
  bool HasActiveEffectivelyFullscreenVideo() override;
  void WriteIntoTrace(perfetto::TracedValue context) override;
  const base::Location& GetCreatorLocation() override;
  const std::optional<blink::mojom::PictureInPictureWindowOptions>&
  GetPictureInPictureOptions() const override;
  void UpdateBrowserControlsState(
      cc::BrowserControlsState constraints,
      cc::BrowserControlsState current,
      bool animate,
      const std::optional<cc::BrowserControlsOffsetTagsInfo>& offset_tags_info)
      override;
  void SetV8CompileHints(base::ReadOnlySharedMemoryRegion data) override;
  void SetTabSwitchStartTime(base::TimeTicks start_time,
                             bool destination_is_loaded) override;
  bool IsInPreviewMode() const override;
  void WillActivatePreviewPage() override;
  void ActivatePreviewPage() override;

  // Implementation of PageNavigator.
  WebContents* OpenURL(const OpenURLParams& params,
                       base::OnceCallback<void(content::NavigationHandle&)>
                           navigation_handle_callback) override;

  const blink::web_pref::WebPreferences& GetOrCreateWebPreferences() override;
  void NotifyPreferencesChanged() override;
  void SetWebPreferences(const blink::web_pref::WebPreferences& prefs) override;
  void OnWebPreferencesChanged() override;

  void AboutToBeDiscarded(WebContents* new_contents) override;
  void NotifyWasDiscarded() override;

  [[nodiscard]] base::ScopedClosureRunner CreateDisallowCustomCursorScope(
      int max_dimension_dips) override;

  void SetOverscrollNavigationEnabled(bool enabled) override;

  // RenderFrameHostDelegate ---------------------------------------------------
  bool OnMessageReceived(RenderFrameHostImpl* render_frame_host,
                         const IPC::Message& message) override;
  void OnDidBlockNavigation(
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  void OnDidFinishLoad(RenderFrameHostImpl* render_frame_host,
                       const GURL& url) override;
  void OnManifestUrlChanged(PageImpl& page) override;
  void RenderFrameCreated(RenderFrameHostImpl* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHostImpl* render_frame_host) override;
  void ShowContextMenu(
      RenderFrameHost& render_frame_host,
      mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient>
          context_menu_client,
      const ContextMenuParams& params) override;
  void RunJavaScriptDialog(RenderFrameHostImpl* render_frame_host,
                           const std::u16string& message,
                           const std::u16string& default_prompt,
                           JavaScriptDialogType dialog_type,
                           bool disable_third_party_subframe_suppresion,
                           JavaScriptDialogCallback response_callback) override;
  void RunBeforeUnloadConfirm(
      RenderFrameHostImpl* render_frame_host,
      bool is_reload,
      JavaScriptDialogCallback response_callback) override;
  void DidChangeName(RenderFrameHostImpl* render_frame_host,
                     const std::string& name) override;
  void DidReceiveUserActivation(
      RenderFrameHostImpl* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      RenderFrameHostImpl* render_frame_host) override;
  void BindDisplayCutoutHost(
      RenderFrameHostImpl* render_frame_host,
      mojo::PendingAssociatedReceiver<blink::mojom::DisplayCutoutHost> receiver)
      override;
  void DidChangeDisplayState(RenderFrameHostImpl* render_frame_host,
                             bool is_display_none) override;
  void FrameSizeChanged(RenderFrameHostImpl* render_frame_host,
                        const gfx::Size& frame_size) override;
  void DOMContentLoaded(RenderFrameHostImpl* render_frame_host) override;
  void DocumentOnLoadCompleted(RenderFrameHostImpl* render_frame_host) override;
  void UpdateTitle(RenderFrameHostImpl* render_frame_host,
                   const std::u16string& title,
                   base::i18n::TextDirection title_direction) override;
  // The app title is an alternative title. If non-empty, the browser may choose
  // to use the app title instead of the regular title for a web app displayed
  // in an app window. See
  // https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/DocumentSubtitle/explainer.md
  void UpdateAppTitle(RenderFrameHostImpl* render_frame_host,
                      const std::u16string& app_title) override;
  void UpdateTargetURL(RenderFrameHostImpl* render_frame_host,
                       const GURL& url) override;
  bool IsNeverComposited() override;
  void SetCaptureHandleConfig(
      blink::mojom::CaptureHandleConfigPtr config) override;
  ui::AXMode GetAccessibilityMode() override;
  // Broadcasts the mode change to all frames.
  void ResetAccessibility() override;
  void AXTreeIDForMainFrameHasChanged() override;
  void ProcessAccessibilityUpdatesAndEvents(
      ui::AXUpdatesAndEvents& details) override;
  void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details) override;
  ui::AXNode* GetAccessibilityRootNode() override;
  std::string DumpAccessibilityTree(
      bool internal,
      std::vector<ui::AXPropertyFilter> property_filters) override;
  std::string DumpAccessibilityTree(
      ui::AXApiType::Type api_type,
      std::vector<ui::AXPropertyFilter> property_filters) override;
  void RecordAccessibilityEvents(
      bool start_recording,
      std::optional<ui::AXEventCallback> callback) override;
  void RecordAccessibilityEvents(
      ui::AXApiType::Type api_type,
      bool start_recording,
      std::optional<ui::AXEventCallback> callback) override;
  void UnrecoverableAccessibilityError() override;
  device::mojom::GeolocationContext* GetGeolocationContext() override;
  device::mojom::WakeLockContext* GetWakeLockContext() override;
#if BUILDFLAG(IS_ANDROID)
  void GetNFC(RenderFrameHost*,
              mojo::PendingReceiver<device::mojom::NFC>) override;
#endif
  bool CanEnterFullscreenMode(RenderFrameHostImpl* requesting_frame) override;
  void EnterFullscreenMode(
      RenderFrameHostImpl* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenMode(bool will_cause_resize) override;
  void FullscreenStateChanged(
      RenderFrameHostImpl* rfh,
      bool is_fullscreen,
      blink::mojom::FullscreenOptionsPtr options) override;
  bool CanUseWindowingControls(RenderFrameHostImpl* requesting_frame) override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
#if BUILDFLAG(IS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override;
#endif
  bool ShouldRouteMessageEvent(RenderFrameHostImpl* target_rfh) const override;
  void EnsureOpenerProxiesExist(RenderFrameHostImpl* source_rfh) override;
  void DidCallFocus() override;
  void OnFocusedElementChangedInFrame(
      RenderFrameHostImpl* frame,
      const gfx::Rect& bounds_in_root_view,
      blink::mojom::FocusType focus_type) override;
  void OnAdvanceFocus(RenderFrameHostImpl* source_rfh) override;
  FrameTree* CreateNewWindow(
      RenderFrameHostImpl* opener,
      const mojom::CreateNewWindowParams& params,
      bool is_new_browsing_instance,
      bool has_user_gesture,
      SessionStorageNamespace* session_storage_namespace) override;
  void ShowCreatedWindow(RenderFrameHostImpl* opener,
                         int main_frame_widget_route_id,
                         WindowOpenDisposition disposition,
                         const blink::mojom::WindowFeatures& window_features,
                         bool user_gesture) override;
  void PrimaryMainDocumentElementAvailable() override;
  void PassiveInsecureContentFound(const GURL& resource_url) override;
  bool ShouldAllowRunningInsecureContent(bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  void ViewSource(RenderFrameHostImpl* frame) override;
  void PrintCrossProcessSubframe(
      const gfx::Rect& rect,
      int document_cookie,
      RenderFrameHostImpl* render_frame_host) override;
  void CapturePaintPreviewOfCrossProcessSubframe(
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      RenderFrameHostImpl* render_frame_host) override;
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHostDelegate()
      override;
#endif
  void ResourceLoadComplete(
      RenderFrameHostImpl* render_frame_host,
      const GlobalRequestID& request_id,
      blink::mojom::ResourceLoadInfoPtr resource_load_information) override;
  void OnCookiesAccessed(RenderFrameHostImpl*,
                         const CookieAccessDetails& details) override;
  void OnTrustTokensAccessed(RenderFrameHostImpl*,
                             const TrustTokenAccessDetails& details) override;
  void OnSharedDictionaryAccessed(
      RenderFrameHostImpl*,
      const network::mojom::SharedDictionaryAccessDetails& details) override;
  void NotifyStorageAccessed(RenderFrameHostImpl*,
                             blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override;
  void OnVibrate(RenderFrameHostImpl*) override;

  std::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(RenderFrameHostImpl* source) override;

  // Called when WebAudio starts or stops playing audible audio in an
  // AudioContext.
  void AudioContextPlaybackStarted(RenderFrameHostImpl* host,
                                   int context_id) override;
  void AudioContextPlaybackStopped(RenderFrameHostImpl* host,
                                   int context_id) override;
  void OnFrameAudioStateChanged(RenderFrameHostImpl* host,
                                bool is_audible) override;
  void OnRemoteSubframeViewportIntersectionStateChanged(
      RenderFrameHostImpl* host,
      const blink::mojom::ViewportIntersectionState&
          viewport_intersection_state) override;
  void OnFrameVisibilityChanged(
      RenderFrameHostImpl* host,
      blink::mojom::FrameVisibility visibility) override;
  void OnFrameIsCapturingMediaStreamChanged(
      RenderFrameHostImpl* host,
      bool is_capturing_media_stream) override;
  std::vector<FrameTreeNode*> GetUnattachedOwnedNodes(
      RenderFrameHostImpl* owner) override;
  void RegisterProtocolHandler(RenderFrameHostImpl* source,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(RenderFrameHostImpl* source,
                                 const std::string& protocol,
                                 const GURL& url,
                                 bool user_gesture) override;
  bool IsAllowedToGoToEntryAtOffset(int32_t offset) override;
  void IsClipboardPasteAllowedByPolicy(
      const ClipboardEndpoint& source,
      const ClipboardEndpoint& destination,
      const ClipboardMetadata& metadata,
      ClipboardPasteData clipboard_paste_data,
      IsClipboardPasteAllowedCallback callback) override;
  void OnTextCopiedToClipboard(RenderFrameHostImpl* render_frame_host,
                               const std::u16string& copied_text) override;
  void IsClipboardPasteAllowedWrapperCallback(
      IsClipboardPasteAllowedCallback callback,
      std::optional<ClipboardPasteData> clipboard_paste_data);
  void OnPageScaleFactorChanged(PageImpl& source) override;
  void BindScreenOrientation(
      RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>
          receiver) override;
  bool IsTransientActivationRequiredForHtmlFullscreen() override;
  bool IsBackForwardCacheSupported() override;
  RenderWidgetHostImpl* CreateNewPopupWidget(
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t route_id,
      mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
          blink_popup_widget_host,
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget)
      override;
  void DidLoadResourceFromMemoryCache(
      RenderFrameHostImpl* source,
      const GURL& url,
      const std::string& http_request,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination,
      bool include_credentials) override;
  void DomOperationResponse(RenderFrameHost* render_frame_host,
                            const std::string& json_string) override;
  void SavableResourceLinksResponse(
      RenderFrameHostImpl* source,
      const std::vector<GURL>& resources_list,
      blink::mojom::ReferrerPtr referrer,
      const std::vector<blink::mojom::SavableSubframePtr>& subframes) override;
  void SavableResourceLinksError(RenderFrameHostImpl* source) override;
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;
  void SetWindowRect(const gfx::Rect& new_bounds) override;
  void UpdateWindowPreferredSize(const gfx::Size& pref_size) override;
  std::vector<RenderFrameHostImpl*>
  GetActiveTopLevelDocumentsInBrowsingContextGroup(
      RenderFrameHostImpl* render_frame_host) override;
  std::vector<RenderFrameHostImpl*>
  GetActiveTopLevelDocumentsInCoopRelatedGroup(
      RenderFrameHostImpl* render_frame_host) override;
  PrerenderHostRegistry* GetPrerenderHostRegistry() override;
#if BUILDFLAG(ENABLE_PPAPI)
  void OnPepperInstanceCreated(RenderFrameHostImpl* source,
                               int32_t pp_instance) override;
  void OnPepperInstanceDeleted(RenderFrameHostImpl* source,
                               int32_t pp_instance) override;
  void OnPepperStartsPlayback(RenderFrameHostImpl* source,
                              int32_t pp_instance) override;
  void OnPepperStopsPlayback(RenderFrameHostImpl* source,
                             int32_t pp_instance) override;
  void OnPepperPluginCrashed(RenderFrameHostImpl* source,
                             const base::FilePath& plugin_path,
                             base::ProcessId plugin_pid) override;
  void OnPepperPluginHung(RenderFrameHostImpl* source,
                          int plugin_child_id,
                          const base::FilePath& path,
                          bool is_hung) override;
#endif  // BUILDFLAG(ENABLE_PPAPI)
  void DidChangeLoadProgressForPrimaryMainFrame() override;
  void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                            const GURL& url,
                            int error_code) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions) override;

  // RenderViewHostDelegate ----------------------------------------------------
  RenderViewHostDelegateView* GetDelegateView() override;
  void RenderViewReady(RenderViewHost* render_view_host) override;
  void RenderViewTerminated(RenderViewHost* render_view_host,
                            base::TerminationStatus status,
                            int error_code) override;
  void RenderViewDeleted(RenderViewHost* render_view_host) override;
  bool DidAddMessageToConsole(
      RenderFrameHostImpl* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;
  const blink::RendererPreferences& GetRendererPrefs() const override;
  void DidReceiveInputEvent(RenderWidgetHostImpl* render_widget_host,
                            const blink::WebInputEvent& event) override;
  bool ShouldIgnoreWebInputEvents(const blink::WebInputEvent& event) override;
  bool ShouldIgnoreInputEvents() override;
  void OnIgnoredUIEvent() override;
  void Activate() override;
  void ShowCreatedWidget(int process_id,
                         int widget_route_id,
                         const gfx::Rect& initial_rect,
                         const gfx::Rect& initial_anchor_rect) override;
  void CreateMediaPlayerHostForRenderFrameHost(
      RenderFrameHostImpl* frame_host,
      mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost> receiver)
      override;
  void RequestMediaAccessPermission(const MediaStreamRequest& request,
                                    MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(RenderFrameHostImpl* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool IsJavaScriptDialogShowing() const override;
  bool ShouldIgnoreUnresponsiveRenderer() override;
  bool IsGuest() override;
  std::optional<SkColor> GetBaseBackgroundColor() override;
  void StartPrefetch(const GURL& prefetch_url,
                     bool use_prefetch_proxy,
                     const blink::mojom::Referrer& referrer,
                     const std::optional<url::Origin>& referring_origin,
                     base::WeakPtr<PreloadingAttempt> attempt,
                     std::optional<PreloadingHoldbackStatus>
                         holdback_status_override) override;
  std::unique_ptr<PrerenderHandle> StartPrerendering(
      const GURL& prerendering_url,
      PreloadingTriggerType trigger_type,
      const std::string& embedder_histogram_suffix,
      ui::PageTransition page_transition,
      bool should_warm_up_compositor,
      PreloadingHoldbackStatus holdback_status_override,
      PreloadingAttempt* preloading_attempt,
      base::RepeatingCallback<bool(const GURL&,
                                   const std::optional<UrlMatchType>&)>,
      base::RepeatingCallback<void(NavigationHandle&)>) override;
  void CancelAllPrerendering() override;
  void BackNavigationLikely(PreloadingPredictor predictor,
                            WindowOpenDisposition disposition) override;
  void SetOwnerLocationForDebug(
      std::optional<base::Location> owner_location) override;
  blink::ColorProviderColorMaps GetColorProviderColorMaps() const override;

  network::mojom::AttributionSupport GetAttributionSupport() override;
  void UpdateAttributionSupportRenderer() override;
  static void UpdateAttributionSupportAllRenderers();
  BackForwardTransitionAnimationManager*
  GetBackForwardTransitionAnimationManager() override;
  net::handles::NetworkHandle GetTargetNetwork() override;

  void GetMediaCaptureRawDeviceIdsOpened(
      blink::mojom::MediaStreamType type,
      base::OnceCallback<void(std::vector<std::string>)> callback) override;

  // NavigatorDelegate ---------------------------------------------------------

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void DidCancelNavigationBeforeStart(
      NavigationHandle* navigation_handle) override;
  void DidNavigateMainFramePreCommit(NavigationHandle* navigation_handle,
                                     bool navigation_is_within_page) override;
  void DidNavigateMainFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details) override;
  void DidNavigateAnyFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details) override;
  void DidUpdateNavigationHandleTiming(
      NavigationHandle* navigation_handle) override;
  void NotifyChangedNavigationState(InvalidateTypes changed_flags) override;
  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_outermost_main_frame_navigation) override;
  std::vector<std::unique_ptr<NavigationThrottle>> CreateThrottlesForNavigation(
      NavigationHandle* navigation_handle) override;
  std::vector<std::unique_ptr<CommitDeferringCondition>>
  CreateDeferringConditionsForNavigationCommit(
      NavigationHandle& navigation_handle,
      CommitDeferringCondition::NavigationType type) override;
  std::unique_ptr<NavigationUIData> GetNavigationUIData(
      NavigationHandle* navigation_handle) override;
  void OnServiceWorkerAccessed(NavigationHandle* navigation,
                               const GURL& scope,
                               AllowServiceWorkerResult allowed) override;
  void OnCookiesAccessed(NavigationHandle*,
                         const CookieAccessDetails& details) override;
  void OnTrustTokensAccessed(NavigationHandle*,
                             const TrustTokenAccessDetails& details) override;
  void OnSharedDictionaryAccessed(
      NavigationHandle*,
      const network::mojom::SharedDictionaryAccessDetails& details) override;
  void RegisterExistingOriginAsHavingDefaultIsolation(
      const url::Origin& origin,
      NavigationRequest* navigation_request_to_exclude) override;
  bool MaybeCopyContentAreaAsBitmap(
      base::OnceCallback<void(const SkBitmap&)> callback) override;

  // RenderWidgetHostDelegate --------------------------------------------------

  void SetTopControlsShownRatio(RenderWidgetHostImpl* render_widget_host,
                                float ratio) override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  void RenderWidgetCreated(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetDeleted(RenderWidgetHostImpl* render_widget_host) override;
  void RenderWidgetWasResized(RenderWidgetHostImpl* render_widget_host,
                              bool width_changed) override;
  void ResizeDueToAutoResize(RenderWidgetHostImpl* render_widget_host,
                             const gfx::Size& new_size) override;
  void OnVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection scroll_direction) override;
  int GetVirtualKeyboardResizeHeight() override;
  bool ShouldDoLearning() override;

  double GetPendingPageZoomLevel() override;

  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleMouseEvent(const blink::WebMouseEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  bool HandleWheelEvent(const blink::WebMouseWheelEvent& event) override;
  bool PreHandleGestureEvent(const blink::WebGestureEvent& event) override;
  ui::BrowserAccessibilityManager* GetRootBrowserAccessibilityManager()
      override;
  ui::BrowserAccessibilityManager* GetOrCreateRootBrowserAccessibilityManager()
      override;
  // The following 4 functions are already listed under WebContents overrides:
  // void Cut() override;
  // void Copy() override;
  // void Paste() override;
  // void SelectAll() override;
  void ExecuteEditCommand(const std::string& command,
                          const std::optional<std::u16string>& value) override;
  void MoveRangeSelectionExtent(const gfx::Point& extent) override;
  void SelectRange(const gfx::Point& base, const gfx::Point& extent) override;
  void SelectAroundCaret(blink::mojom::SelectionGranularity granularity,
                         bool should_show_handle,
                         bool should_show_context_menu) override;
  void MoveCaret(const gfx::Point& extent) override;
  uint32_t GetCompositorFrameSinkGroupingId() const override;
  void AdjustSelectionByCharacterOffset(int start_adjust,
                                        int end_adjust,
                                        bool show_selection_menu) override;
  input::RenderWidgetHostInputEventRouter* GetInputEventRouter() override;
  void GetRenderWidgetHostAtPointAsynchronously(
      RenderWidgetHostViewBase* root_view,
      const gfx::PointF& point,
      base::OnceCallback<void(base::WeakPtr<RenderWidgetHostViewBase>,
                              std::optional<gfx::PointF>)> callback) override;
  RenderWidgetHostImpl* GetFocusedRenderWidgetHost(
      RenderWidgetHostImpl* receiving_widget) override;
  RenderWidgetHostImpl* GetRenderWidgetHostWithPageFocus() override;
  void FocusOwningWebContents(
      RenderWidgetHostImpl* render_widget_host) override;
  void RendererUnresponsive(
      RenderWidgetHostImpl* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void RendererResponsive(RenderWidgetHostImpl* render_widget_host) override;
  void RequestToLockPointer(RenderWidgetHostImpl* render_widget_host,
                            bool user_gesture,
                            bool last_unlocked_by_target,
                            bool privileged) override;
  bool IsWaitingForPointerLockPrompt(
      RenderWidgetHostImpl* render_widget_host) override;
  bool RequestKeyboardLock(RenderWidgetHostImpl* render_widget_host,
                           bool esc_key_locked) override;
  void CancelKeyboardLock(RenderWidgetHostImpl* render_widget_host) override;
  RenderWidgetHostImpl* GetKeyboardLockWidget() override;
  // The following function is already listed under WebContents overrides:
  // bool IsFullscreen() const override;
  blink::mojom::DisplayMode GetDisplayMode() const override;
  ui::mojom::WindowShowState GetWindowShowState() override;
  blink::mojom::DevicePostureProvider* GetDevicePostureProvider() override;
  bool GetResizable() override;
  void LostPointerLock(RenderWidgetHostImpl* render_widget_host) override;
  bool HasPointerLock(RenderWidgetHostImpl* render_widget_host) override;
  RenderWidgetHostImpl* GetPointerLockWidget() override;
  bool OnRenderFrameProxyVisibilityChanged(
      RenderFrameProxyHost* render_frame_proxy_host,
      blink::mojom::FrameVisibility visibility) override;
  void SendScreenRects() override;
  void SendActiveState(bool active) override;
  TextInputManager* GetTextInputManager() override;
  bool IsWidgetForPrimaryMainFrame(
      RenderWidgetHostImpl* render_widget_host) override;
  bool IsShowingContextMenuOnPage() const override;
  void DidChangeScreenOrientation() override;
  gfx::Rect GetWindowsControlsOverlayRect() const override;
  VisibleTimeRequestTrigger& GetVisibleTimeRequestTrigger() final;
  gfx::mojom::DelegatedInkPointRenderer* GetDelegatedInkRenderer(
      ui::Compositor* compositor) override;
  void OnInputIgnored(const blink::WebInputEvent& event) override;

  // RenderFrameHostManager::Delegate ------------------------------------------

  bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host,
      const std::optional<blink::FrameToken>& opener_frame_token,
      RenderFrameProxyHost* proxy_host) override;
  void ReattachOuterDelegateIfNeeded() override;
  void CreateRenderWidgetHostViewForRenderManager(
      RenderViewHost* render_view_host) override;
  void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      bool* proceed_to_fire_unload) override;
  void CancelModalDialogsForRenderManager() override;
  void NotifySwappedFromRenderManager(RenderFrameHostImpl* old_frame,
                                      RenderFrameHostImpl* new_frame) override;
  void NotifySwappedFromRenderManagerWithoutFallbackContent(
      RenderFrameHostImpl* new_frame) override;
  void NotifyMainFrameSwappedFromRenderManager(
      RenderFrameHostImpl* old_frame,
      RenderFrameHostImpl* new_frame) override;
  bool FocusLocationBarByDefault() override;
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override;

  // PageDelegate -------------------------------------------------------------

  void OnFirstVisuallyNonEmptyPaint(PageImpl& page) override;

  // These both check that the color has in fact changed before notifying
  // observers.
  void OnThemeColorChanged(PageImpl& page) override;
  void OnBackgroundColorChanged(PageImpl& page) override;
  void DidInferColorScheme(PageImpl& page) override;
  void OnVirtualKeyboardModeChanged(PageImpl& page) override;
  void NotifyPageBecamePrimary(PageImpl& page) override;

  bool IsPageInPreviewMode() const override;
  void CancelPreviewByMojoBinderPolicy(
      const std::string& interface_name) override;
  void OnCanResizeFromWebAPIChanged() override;

  // blink::mojom::ColorChooserFactory ---------------------------------------
  void OnColorChooserFactoryReceiver(
      mojo::PendingReceiver<blink::mojom::ColorChooserFactory> receiver);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void OpenColorChooser(
      mojo::PendingReceiver<blink::mojom::ColorChooser> chooser,
      mojo::PendingRemote<blink::mojom::ColorChooserClient> client,
      SkColor color,
      std::vector<blink::mojom::ColorSuggestionPtr> suggestions) override;
#endif

  // FrameTree::Delegate -------------------------------------------------------

  void LoadingStateChanged(LoadingState new_state) override;
  void DidStartLoading(FrameTreeNode* frame_tree_node) override;
  void DidStopLoading() override;
  bool IsHidden() override;
  FrameTreeNodeId GetOuterDelegateFrameTreeNodeId() override;
  RenderFrameHostImpl* GetProspectiveOuterDocument() override;
  FrameTree* LoadingTree() override;
  void SetFocusedFrame(FrameTreeNode* node, SiteInstanceGroup* source) override;
  FrameTree* GetOwnedPictureInPictureFrameTree() override;
  FrameTree* GetPictureInPictureOpenerFrameTree() override;

  // NavigationControllerDelegate ----------------------------------------------

  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override;
  void NotifyNavigationListPruned(const PrunedDetails& pruned_details) override;
  void NotifyNavigationEntriesDeleted() override;
  bool ShouldPreserveAbortedURLs() override;
  void NotifyNavigationStateChangedFromController(
      InvalidateTypes changed_flags) override;

  //  RenderWidgetHostInputEventRouter::Delegate -------------------------------
  input::TouchEmulator* GetTouchEmulator(bool create_if_necessary) override;

  // Invoked before a form repost warning is shown.
  void NotifyBeforeFormRepostWarningShow() override;

  // Activate this WebContents and show a form repost warning.
  void ActivateAndShowRepostFormWarningDialog() override;

  void MediaMutedStatusChanged(const MediaPlayerId& id, bool muted);

  void UpdateOverridingUserAgent() override;

  // Forces overscroll to be disabled (used by touch emulation).
  void SetForceDisableOverscrollContent(bool force_disable);

  // Override the render view/widget size of the main frame, return whether the
  // size changed.
  bool SetDeviceEmulationSize(const gfx::Size& new_size);
  void ClearDeviceEmulationSize();

  AudioStreamMonitor* audio_stream_monitor() { return &audio_stream_monitor_; }

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

  // Called by MediaWebContentsObserver when a buffer underflow occurs. See the
  // WebContentsObserver function stubs for more details.
  void MediaBufferUnderflow(const MediaPlayerId& id);

  // Called by MediaWebContentsObserver when player seek event occurs.
  void MediaPlayerSeek(const MediaPlayerId& id);

  // Called by MediaWebContentsObserver when a media player is destroyed.
  void MediaDestroyed(const MediaPlayerId& id);

  // Called by MediaSessionImpl when one is created and initialized for this.
  void MediaSessionCreated(MediaSession* media_session);

  int GetCurrentlyPlayingVideoCount() override;
  std::optional<gfx::Size> GetFullscreenVideoSize() override;

  MediaWebContentsObserver* media_web_contents_observer() {
    return media_web_contents_observer_.get();
  }

  // Update the web contents visibility.
  void UpdateWebContentsVisibility(Visibility visibility) override;

  // Returns the PageVisibilityState for the primary page of this web contents,
  // taking the capturing state into account.
  PageVisibilityState GetPageVisibilityState() const;

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

  void IncrementBluetoothScanningSessionsCount();
  void DecrementBluetoothScanningSessionsCount();

  // Modify the counter of frames in this WebContents actively using serial
  // ports.
  void IncrementSerialActiveFrameCount();
  void DecrementSerialActiveFrameCount();

  // Modify the counter of frames in this WebContents actively using HID
  // devices.
  void IncrementHidActiveFrameCount();
  void DecrementHidActiveFrameCount();

  // Notifies the delegate and observers when device connection types used by
  // the WebContents change.
  void OnDeviceConnectionTypesChanged(
      WebContentsObserver::DeviceConnectionType device_connection_type,
      bool used);

  // Modify the counter of frames in this WebContents actively using USB
  // devices.
  void IncrementUsbActiveFrameCount();
  void DecrementUsbActiveFrameCount();

  // Modify the counter of File System Access handles for this WebContents.
  void IncrementFileSystemAccessHandleCount();
  void DecrementFileSystemAccessHandleCount();

  // Called when the WebContents gains or loses a persistent video.
  void SetHasPersistentVideo(bool has_persistent_video);

  // Whether the WebContents effectively fullscreen active player allows
  // Picture-in-Picture.
  // |IsFullscreen| must return |true| when this method is called.
  bool IsPictureInPictureAllowedForFullscreenVideo() const;

  // Set this WebContents's `primary_frame_tree_` as the focused frame tree.
  // `primary_frame_tree_`'s main frame RenderWidget (and all of its
  // subframe widgets) will be activated. GetFocusedRenderWidgetHost will search
  // this WebContentsImpl for a focused RenderWidgetHost. The previously focused
  // WebContentsImpl, if any, will have its RenderWidgetHosts deactivated.
  void SetAsFocusedWebContentsIfNecessary();

  // Sets the focused frame tree to be the `frame_tree_to_focus`.
  // `frame_tree_to_focus` must be either this WebContents's frame tree or
  // contained within it (but not owned by another WebContents).
  void SetFocusedFrameTree(FrameTree* frame_tree_to_focus);

  // Notifies the Picture-in-Picture controller that there is a new video player
  // entering video Picture-in-Picture. (This is not used for document
  // Picture-in-Picture,
  // cf. PictureInPictureWindowManager::EnterDocumentPictureInPicture().)
  // Returns the result of the enter request.
  PictureInPictureResult EnterPictureInPicture();

  // Updates the Picture-in-Picture controller with a signal that
  // Picture-in-Picture mode has ended.
  void ExitPictureInPicture();

  // Updates the tracking information for |this| to know if there is
  // a video currently in Picture-in-Picture mode.
  void SetHasPictureInPictureVideo(bool has_picture_in_picture_video);

  // Updates the tracking information for |this| to know if there is
  // a document currently in Picture-in-Picture mode.
  void SetHasPictureInPictureDocument(bool has_picture_in_picture_document);

  // Sets the spatial navigation state.
  void SetSpatialNavigationDisabled(bool disabled);

  // Sets the Stylus handwriting feature status. This status is updated to web
  // preferences.
  void SetStylusHandwritingEnabled(bool enabled);

  // Called when a file selection is to be done.
  void RunFileChooser(
      base::WeakPtr<FileChooserImpl> file_chooser,
      RenderFrameHost* render_frame_host,
      scoped_refptr<FileChooserImpl::FileSelectListenerImpl> listener,
      const blink::mojom::FileChooserParams& params);

  // Request to enumerate a directory.  This is equivalent to running the file
  // chooser in directory-enumeration mode and having the user select the given
  // directory.
  void EnumerateDirectory(
      base::WeakPtr<FileChooserImpl> file_chooser,
      RenderFrameHost* render_frame_host,
      scoped_refptr<FileChooserImpl::FileSelectListenerImpl> listener,
      const base::FilePath& directory_path);

#if BUILDFLAG(IS_ANDROID)
  // Called by FindRequestManager when all of the find match rects are in.
  void NotifyFindMatchRectsReply(int version,
                                 const std::vector<gfx::RectF>& rects,
                                 const gfx::RectF& active_rect);
#endif

#if BUILDFLAG(IS_ANDROID)
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

  // Convenience method to notify observers that an inner WebContents was
  // created with |this| WebContents as its owner. This does *not* immediately
  // guarantee that |inner_web_contents| has been added to the WebContents tree.
  void InnerWebContentsCreated(WebContents* inner_web_contents);

  // Reattaches this inner WebContents to its outer WebContents.
  virtual void ReattachToOuterWebContentsFrame();

  // Notifies observers that this WebContents completed preview activation
  // steps.
  // `activation_time` is the time the activation happened, in wall time.
  void DidActivatePreviewedPage(base::TimeTicks activation_time);

  void OnServiceWorkerAccessed(RenderFrameHost* render_frame_host,
                               const GURL& scope,
                               AllowServiceWorkerResult allowed);

  bool JavaScriptDialogDefersNavigations() {
    return javascript_dialog_dismiss_notifier_.get();
  }

  void NotifyOnJavaScriptDialogDismiss(base::OnceClosure callback);

  bool has_persistent_video() { return has_persistent_video_; }

  // Returns the focused frame's input handler.
  blink::mojom::FrameWidgetInputHandler* GetFocusedFrameWidgetInputHandler();

  // A render view-originated drag has ended. Informs the render view host and
  // WebContentsDelegate.
  void SystemDragEnded(RenderWidgetHost* source_rwh);

  // These are the content internal equivalents of
  // |WebContents::ForEachRenderFrameHost| whose comment can be referred to
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

  // Computes and returns the content specific preferences for this WebContents.
  // Recomputes only the "fast" preferences (those not requiring slow
  // platform/device polling); the remaining "slow" ones are recomputed only if
  // the preference cache is empty.
  const blink::web_pref::WebPreferences ComputeWebPreferences();

  // Certain WebXr modes integrate with Viz as a compositor directly, and thus
  // have their own FrameSinkId that typically renders fullscreen, obscuring
  // the WebContents. This allows those WebXr modes to notify us if that
  // is occurring. When it has finished, this method may be called again with an
  // Invalid FrameSinkId to indicate such. Note that other fullscreen modes,
  // e.g. Fullscreen videos, are largely controlled by the renderer process and
  // as such are still parented under the existing FrameSinkId.
  void OnXrHasRenderTarget(const viz::FrameSinkId& frame_sink_id);

  // Because something else may be rendering as the primary contents of this
  // WebContents rather than the RenderHostView, targets that wish to capture
  // the contents of this WebContents should query its capture target here.
  struct CaptureTarget {
    viz::FrameSinkId sink_id;
    gfx::NativeView view;
  };
  CaptureTarget GetCaptureTarget();

  // Sets the value in tests to ensure expected ordering and correctness.
  void set_minimum_delay_between_loading_updates_for_testing(
      base::TimeDelta duration) {
    minimum_delay_between_loading_updates_ms_ = duration;
  }

  // If the given frame is prerendered, cancels the associated prerender.
  // Returns true if a prerender was canceled.
  bool CancelPrerendering(FrameTreeNode* frame_tree_node,
                          PrerenderFinalStatus final_status);

  void set_suppress_ime_events_for_testing(bool suppress) {
    suppress_ime_events_for_testing_ = suppress;
  }

  RenderWidgetHost* mouse_lock_widget_for_testing() {
    return pointer_lock_widget_;
  }

  ui::mojom::VirtualKeyboardMode GetVirtualKeyboardMode() const;

  const std::optional<base::Location>& ownership_location() const {
    return ownership_location_;
  }

  bool IsPopup() const override;

  bool IsPartitionedPopin() const override;

  RenderFrameHostImpl* PartitionedPopinOpener() const override;

  WebContents* OpenedPartitionedPopin() const override;

 private:
  using FrameTreeIterationCallback = base::RepeatingCallback<void(FrameTree&)>;
  using RenderViewHostIterationCallback =
      base::RepeatingCallback<void(RenderViewHostImpl*)>;

  friend class WebContentsObserver;
  friend class WebContents;  // To implement factory methods.

  friend class RenderFrameHostImplBeforeUnloadBrowserTest;
  friend class WebContentsImplBrowserTest;
  friend class TestWebContentsDestructionObserver;
  friend class BeforeUnloadBlockingDelegate;
  friend class TestWCDelegateForDialogsAndFullscreen;

  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, CaptureHoldsWakeLock);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, NoJSMessageOnInterstitials);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, UpdateTitle);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FindOpenerRVHWhenPending);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           CrossSiteCantPreemptAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, PendingContentsDestroyed);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, PendingContentsShown);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FrameTreeShape);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           NonActivityCaptureDoesNotCountAsActivity);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, GetLastActiveTimeTicks);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           LoadResourceFromMemoryCacheWithBadSecurityInfo);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           LoadResourceWithEmptySecurityInfo);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           ResetJavaScriptDialogOnUserNavigate);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, ParseDownloadHeaders);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FaviconURLsSet);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FaviconURLsResetWithNavigation);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, FaviconURLsUpdateDelay);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           NotifyFullscreenAcquired);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           NotifyFullscreenAcquired_Navigate);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           NotifyFullscreenAcquired_SameOrigin);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           PropagateFullscreenOptions);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           FullscreenAfterFrameUnload);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           MaxFrameCountForCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           MaxFrameCountRemovedIframes);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           MaxFrameCountInjectedIframes);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           ForEachFrameTreeInnerContents);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           UserAgentOverrideDuringDeferredNavigation);
  FRIEND_TEST_ALL_PREFIXES(FencedFrameMPArchBrowserTest, FrameIteration);
  FRIEND_TEST_ALL_PREFIXES(FencedFrameParameterizedBrowserTest,
                           ShouldIgnoreJsDialog);
  FRIEND_TEST_ALL_PREFIXES(FormStructureBrowserTest, HTMLFiles);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest, HistoryNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, PageDoesBackAndReload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrossSiteIframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           TwoSubframesCreatePopupsSimultaneously);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, TextAutosizerPageInfo);
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
                           BeforeUnloadConfirmOnNonActive);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           PendingDialogMakesDiscardUnloadReturnFalse);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, JavaScriptDialogNotifications);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, JavaScriptDialogInterop);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, BeforeUnloadDialog);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest, PageDisableWithOpenedDialog);
  FRIEND_TEST_ALL_PREFIXES(DevToolsProtocolTest,
                           PageDisableWithNoDialogManager);
  FRIEND_TEST_ALL_PREFIXES(
      PrerenderWithRenderDocumentBrowserTest,
      ModalDialogShouldNotBeDismissedAfterPrerenderSubframeNavigation);
  FRIEND_TEST_ALL_PREFIXES(PrerenderBrowserTest, ForEachRenderFrameHost);
  FRIEND_TEST_ALL_PREFIXES(PluginContentOriginAllowlistTest,
                           ClearAllowlistOnNavigate);
  FRIEND_TEST_ALL_PREFIXES(PluginContentOriginAllowlistTest,
                           SubframeInheritsAllowlist);
  FRIEND_TEST_ALL_PREFIXES(PointerLockBrowserTest,
                           PointerLockInnerContentsCrashes);
  FRIEND_TEST_ALL_PREFIXES(PointerLockBrowserTest, PointerLockOopifCrashes);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           PopupWindowBrowserNavResumeLoad);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           SuppressedPopupWindowBrowserNavResumeLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostSitePerProcessTest,
                           BrowserClosesPopupIntersectsPermissionPrompt);

  // So |find_request_manager_| can be accessed for testing.
  friend class FindRequestManagerTest;

  // TODO(brettw) TestWebContents shouldn't exist!
  friend class TestWebContents;

  class RenderWidgetHostDestructionObserver;
  class WebContentsDestructionObserver;

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

    WebContentsImpl* outer_web_contents() const { return outer_web_contents_; }
    FrameTreeNodeId outer_contents_frame_tree_node_id() const {
      return outer_contents_frame_tree_node_id_;
    }
    FrameTreeNode* OuterContentsFrameTreeNode() const;

    FrameTree* focused_frame_tree();
    void SetFocusedFrameTree(FrameTree* frame_tree);

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
    const raw_ptr<WebContentsImpl, DanglingUntriaged> current_web_contents_;

    // The outer WebContents of |current_web_contents_|, or nullptr if
    // |current_web_contents_| is the outermost WebContents.
    raw_ptr<WebContentsImpl, DanglingUntriaged> outer_web_contents_;

    // The ID of the FrameTreeNode in the |outer_web_contents_| that hosts
    // |current_web_contents_| as an inner WebContents.
    FrameTreeNodeId outer_contents_frame_tree_node_id_;

    // List of inner WebContents that we host. The outer WebContents owns the
    // inner WebContents.
    std::vector<std::unique_ptr<WebContents>> inner_web_contents_;

    // Only the root node should have this set. This indicates the FrameTree
    // that has the focused frame. The FrameTree tree could be arbitrarily deep.
    // An inner WebContents if focused is responsible for setting this back to
    // another valid during its destruction. See WebContentsImpl destructor.
    // TODO(crbug.com/40200744): Support clearing this for inner frame trees.
    raw_ptr<FrameTree> focused_frame_tree_;
  };

  // Container for WebContentsObservers, which knows when we are iterating over
  // observer set.
  class WebContentsObserverList {
   public:
    WebContentsObserverList();
    ~WebContentsObserverList();

    void AddObserver(WebContentsObserver* observer);
    void RemoveObserver(WebContentsObserver* observer);

    // T1 must be a pointer to a WebContentsObserver method.
    template <typename T1, typename... P1>
    void NotifyObservers(T1 func, P1&&... args) {
      TRACE_EVENT0("content", "WebContentsObserverList::NotifyObservers");
      base::AutoReset<bool> scope(&is_notifying_observers_, true);
      for (WebContentsObserver& observer : observers_) {
        TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                     "Dispatching WebContentsObserver callback");
        ((observer).*(func))(std::forward<P1>(args)...);
      }
    }

    bool is_notifying_observers() { return is_notifying_observers_; }

    // Exposed to deal with IPC message handlers which need to stop iteration
    // early.
    const base::ObserverList<WebContentsObserver>& observer_list() {
      return observers_;
    }

   private:
    bool is_notifying_observers_ = false;
    base::ObserverList<WebContentsObserver> observers_;
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

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  void SetPrimaryMainFrameProcessStatus(base::TerminationStatus status,
                                        int error_code);

  // Clears a pending contents that has been closed before being shown.
  void OnWebContentsDestroyed(WebContentsImpl* web_contents);

  // Creates and adds to the map a destruction observer watching
  // `render_widget_host`. There must be no observer already watching
  // `render_widget_host`.
  void AddRenderWidgetHostDestructionObserver(
      RenderWidgetHost* render_widget_host);

  // Deletes and removes from the map a destruction observer
  // watching `render_widget_host`. No-op if there is no such observer.
  void RemoveRenderWidgetHostDestructionObserver(
      RenderWidgetHost* render_widget_host);

  // Clears a pending render widget host that has been closed before being
  // shown.
  void OnRenderWidgetHostDestroyed(RenderWidgetHost* render_widget_host);

  // Creates and adds to the map a destruction observer watching `web_contents`.
  // No-op if such an observer already exists.
  void AddWebContentsDestructionObserver(WebContentsImpl* web_contents);

  // Deletes and removes from the map a destruction observer
  // watching `web_contents`. No-op if there is no such observer.
  void RemoveWebContentsDestructionObserver(WebContentsImpl* web_contents);

  // Traverses all the WebContents in the WebContentsTree and creates a set of
  // all the unique RenderWidgetHostViews.
  std::set<RenderWidgetHostViewBase*>
  GetRenderWidgetHostViewsInWebContentsTree();

  // Called with the result of a DownloadImage() request.
  void OnDidDownloadImage(base::WeakPtr<RenderFrameHostImpl> rfh,
                          ImageDownloadCallback callback,
                          int id,
                          const GURL& image_url,
                          int32_t http_status_code,
                          const std::vector<SkBitmap>& images,
                          const std::vector<gfx::Size>& original_image_sizes);

  int GetNextDownloadId();

  // Callback function when showing JavaScript dialogs. Takes in a routing ID
  // pair to identify the RenderFrameHost that opened the dialog, because it's
  // possible for the RenderFrameHost to be deleted by the time this is called.
  void OnDialogClosed(int render_process_id,
                      int render_frame_id,
                      JavaScriptDialogCallback response_callback,
                      base::ScopedClosureRunner fullscreen_block,
                      bool dialog_was_suppressed,
                      bool success,
                      const std::u16string& user_input);

  // IPC message handlers.
  void OnUpdateZoomLimits(RenderViewHostImpl* source,
                          int minimum_percent,
                          int maximum_percent);
  void OnShowValidationMessage(RenderViewHostImpl* source,
                               const gfx::Rect& anchor_in_root_view,
                               const std::u16string& main_text,
                               const std::u16string& sub_text);
  void OnHideValidationMessage(RenderViewHostImpl* source);
  void OnMoveValidationMessage(RenderViewHostImpl* source,
                               const gfx::Rect& anchor_in_root_view);

  // Determines if content is allowed to overscroll. This value comes from the
  // WebContentsDelegate, but can also be overridden by the WebContents.
  bool CanOverscrollContent() const;

  // void CastToViewBaseSafely(
  //     RenderWidgetTargeter::RenderWidgetHostAtPointCallback callback,
  //     base::WeakPtr<RenderWidgetHostViewInput> view,
  //     std::optional<gfx::PointF> point) ;

  // Inner WebContents Helpers -------------------------------------------------
  //
  // These functions are helpers in managing a hierarchy of WebContents
  // involved in rendering inner WebContents.

  // The following functions update registrations for all RenderWidgetHostViews
  // rooted at this WebContents. They are used when attaching/detaching an inner
  // WebContents.
  //
  // Some properties of RenderWidgetHostViews, such as the FrameSinkId and
  // TextInputManager, depend on the outermost WebContents, and must be updated
  // during attach/detach.
  void RecursivelyRegisterRenderWidgetHostViews();
  void RecursivelyUnregisterRenderWidgetHostViews();

  // When multiple WebContents are present within a tab or window, a single one
  // is focused and will route keyboard events in most cases to a RenderWidget
  // contained within it. |GetFocusedWebContents()|'s main frame widget will
  // receive page focus and blur events when the containing window changes focus
  // state.

  // Returns true if |this| is the focused WebContents or an ancestor of the
  // focused WebContents.
  bool ContainsOrIsFocusedWebContents();

  // Called just after an inner web contents is attached.
  void InnerWebContentsAttached(WebContents* inner_web_contents);

  // Called just after an inner web contents is detached.
  void InnerWebContentsDetached(WebContents* inner_web_contents);

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.

  // Finds the new RenderWidgetHost and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  RenderWidgetHostView* GetCreatedWidget(int process_id, int route_id);

  // Finds the new CreatedWindow by |main_frame_widget_route_id|, initializes
  // it for renderer-initiated creation, and returns it. Note that this can only
  // be called once as this call also removes it from the internal map.
  std::optional<CreatedWindow> GetCreatedWindow(int process_id,
                                                int main_frame_widget_route_id);

  // Execute a PageBroadcast Mojo method.
  void ExecutePageBroadcastMethod(PageBroadcastMethodCallback callback);

  // Execute a PageBroadcast Mojo method for all MPArch pages.
  void ExecutePageBroadcastMethodForAllPages(
      PageBroadcastMethodCallback callback);

  void SetOpenerForNewContents(FrameTreeNode* opener, bool opener_suppressed);

  // Tracking loading progress -------------------------------------------------

  // Resets the tracking state of the current load progress.
  void ResetLoadProgressState();

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress();

  // Misc non-view stuff -------------------------------------------------------

  // Sets the history for a specified RenderViewHost to |history_length|
  // entries, with an offset of |history_offset|.
  void SetHistoryOffsetAndLengthForView(RenderViewHost* render_view_host,
                                        int history_offset,
                                        int history_length);

  // Helper functions for sending notifications.
  void NotifyViewSwapped(RenderViewHost* old_view, RenderViewHost* new_view);
  void NotifyFrameSwapped(RenderFrameHostImpl* old_frame,
                          RenderFrameHostImpl* new_frame);

  // TODO(creis): This should take in a FrameTreeNode to know which node's
  // render manager to return.  For now, we just return the root's.
  RenderFrameHostManager* GetRenderManager();

  // Removes browser plugin embedder if there is one.
  void RemoveBrowserPluginEmbedder();

  // Returns the size that the main frame should be sized to.
  gfx::Size GetSizeForMainFrame();

  // Helper method that's called whenever |preferred_size_| or
  // |preferred_size_for_capture_| changes, to propagate the new value to the
  // |delegate_|.
  void OnPreferredSizeChanged(const gfx::Size& old_size);

  void SetJavaScriptDialogManagerForTesting(
      JavaScriptDialogManager* dialog_manager);

  // Returns the FindRequestManager, which may be found in an outer WebContents.
  FindRequestManager* GetFindRequestManager();

  // Returns the FindRequestManager, or tries to create one if it doesn't
  //  already exist. The FindRequestManager may be found in an outer
  // WebContents. If this is an inner WebContents which is not yet attached to
  // an outer WebContents the method will return nullptr.
  FindRequestManager* GetOrCreateFindRequestManager();

  // Prints a console warning when visiting a localhost site with a bad
  // certificate via --allow-insecure-localhost.
  void ShowInsecureLocalhostWarningIfNeeded(PageImpl& page);

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

  // Called each time |fullscreen_frames_| is updated. Find the new
  // |current_fullscreen_frame_id_| and notify observers whenever it changes.
  void FullscreenFrameSetUpdated();

  // Adjusts bounds for minimum window size and available screen area
  // constraints. This compliments similar renderer-side adjustments, using the
  // resolved display mode for new windows, which renderers may be unable to
  // determine.
  int64_t AdjustWindowRect(gfx::Rect* bounds, RenderFrameHostImpl* opener);

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;
  void OnCaptionStyleUpdated() override;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  // Returns the ColorProvider instance for this WebContents object. This will
  // always return a valid ColorProvider instance.
  const ui::ColorProvider& GetColorProvider() const override;

  // implements SlowWebPreferenceCacheObserver
  void OnSlowWebPreferenceChanged() override;

  // Sets the visibility to |new_visibility| and propagates this to the
  // renderer side, taking into account the current capture state. This
  // can be called with the current visibility to affect capturing
  // changes.
  // |is_activity| controls whether a change to |visible| affects
  // the value returned by GetLastActiveTimeTicks().
  void UpdateVisibilityAndNotifyPageAndView(Visibility new_visibility,
                                            bool is_activity = true);

  // Returns UKM source id for the currently displayed page.
  // Intentionally kept private, prefer using
  // render_frame_host->GetPageUkmSourceId() if you already have a
  // |render_frame_host| reference or
  // GetPrimaryMainFrame()->GetPageUkmSourceId() if you don't.
  ukm::SourceId GetCurrentPageUkmSourceId() override;

  // Bit mask to indicate what types of RenderViewHosts to be returned in
  // ForEachRenderViewHost.
  enum ForEachRenderViewHostTypes {
    kPrerenderViews = 1 << 0,
    kBackForwardCacheViews = 1 << 1,
    kActiveViews = 1 << 2,
    kAllViews = kActiveViews | kBackForwardCacheViews | kPrerenderViews,
  };

  // For each RenderViewHost (including bfcache, prerendering) call the
  // callback, this will be filtered by `view_mask`.
  void ForEachRenderViewHost(
      ForEachRenderViewHostTypes view_mask,
      RenderViewHostIterationCallback on_render_view_host);

  // This is the actual implementation of the various overloads of
  // |ForEachRenderFrameHost|.
  void ForEachRenderFrameHostImpl(
      base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame,
      bool include_speculative);

  // Calls |on_frame_tree| for every FrameTree in this WebContents.
  // This does not descend into inner WebContents, but does include inner frame
  // trees based on MPArch.
  void ForEachFrameTree(FrameTreeIterationCallback on_frame_tree);

  // Returns the primary frame tree, followed by any other outermost frame trees
  // in this WebContents. Outermost frame trees include, for example,
  // prerendering frame trees, and do not include, for example, fenced frames.
  // Also note that bfcached pages do not have a distinct frame tree,
  // so the primary frame tree in the result would be the only FrameTree
  // representing any bfcached pages.
  std::vector<FrameTree*> GetOutermostFrameTrees();

  // Returns the primary main frame, followed by the main frames of any other
  // outermost frame trees in this WebContents and the main frames of any
  // bfcached pages. Note that unlike GetOutermostFrameTrees, bfcached pages
  // have a distinct RenderFrameHostImpl in this result.
  std::vector<RenderFrameHostImpl*> GetOutermostMainFrames();

  // Called when the base::ScopedClosureRunner returned by
  // IncrementCapturerCount() is destructed.
  void DecrementCapturerCount(bool stay_hidden,
                              bool stay_awake,
                              bool is_activity = true);

  // Calculates the PageVisibilityState for |visibility|, taking the capturing
  // state into account.
  PageVisibilityState CalculatePageVisibilityState(Visibility visibility) const;

  // Called when the process hosting the primary main RenderFrameHost is known
  // to be alive.
  void NotifyPrimaryMainFrameProcessIsAlive();

  // Updates |entry|'s title. |entry| must belong to the WebContents' primary
  // NavigationController. Returns true if |entry|'s title was changed, and
  // false otherwise.
  bool UpdateTitleForEntryImpl(NavigationEntryImpl* entry,
                               const std::u16string& title);
  // Dispatches WebContentsObserver::TitleWasSet and also notifies the delegate
  // of a title change if |entry| is the entry whose title is being used as the
  // display title.
  void NotifyTitleUpdateForEntry(NavigationEntryImpl* entry);
  // Returns the navigation entry whose title is used as the display title for
  // this WebContents (i.e. for WebContents::GetTitle()).
  NavigationEntry* GetNavigationEntryForTitle();

  // Apply shared logic for SetHasPictureInPictureVideo() and
  // SetHasPictureInPictureDocument().
  void SetHasPictureInPictureCommon(bool has_picture_in_picture);

  // A scope that disallows custom cursors has expired.
  void DisallowCustomCursorScopeExpired();

  // WarmUp a spare render process for future navigations.
  void WarmUpAndroidSpareRenderer();

  // If the new window will be a partitioned popin, we need to validate the
  // settings and set the opener.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  void SetPartitionedPopinOpenerOnNewWindowIfNeeded(
      WebContentsImpl* new_window,
      const mojom::CreateNewWindowParams& params,
      RenderFrameHostImpl* opener);

  // Describes the different types of groups we can be interested in when
  // looking for scriptable frames.
  enum class GroupType { kBrowsingContextGroup, kCoopRelatedGroup };

  // Returns a vector of all the top-level active frames in the same group type
  // specified by `group_type`.
  std::vector<RenderFrameHostImpl*> GetActiveTopLevelDocumentsInGroup(
      RenderFrameHostImpl* render_frame_host,
      GroupType group_type);

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  raw_ptr<WebContentsDelegate, DanglingUntriaged> delegate_;

  // The corresponding view.
  std::unique_ptr<WebContentsView> view_;

  // The view of the RVHD. Usually this is our WebContentsView implementation,
  // but if an embedder uses a different WebContentsView, they'll need to
  // provide this.
  raw_ptr<RenderViewHostDelegateView> render_view_host_delegate_view_;

  // Tracks CreatedWindow objects that have not been shown yet. They are
  // identified by the process ID and routing ID passed to CreateNewWindow.
  std::map<GlobalRoutingID, CreatedWindow> pending_contents_;

  // Watches for the destruction of items in `pending_contents_`.
  std::map<WebContentsImpl*, std::unique_ptr<WebContentsDestructionObserver>>
      web_contents_destruction_observers_;

  // This map holds widgets that were created on behalf of the renderer but
  // haven't been shown yet.
  std::map<GlobalRoutingID, raw_ptr<RenderWidgetHost, CtnExperimental>>
      pending_widgets_;

  // Watches for the destruction of items in `pending_widgets_`.
  std::map<RenderWidgetHost*,
           std::unique_ptr<RenderWidgetHostDestructionObserver>>
      render_widget_host_destruction_observers_;

  // A list of observers notified when page state changes. Weak references.
  // This MUST be listed above `primary_frame_tree_` since at destruction time
  // the latter might cause RenderViewHost's destructor to call us and we might
  // use the observer list then.
  WebContentsObserverList observers_;

  // True if this tab was opened by another window. This is true even if the tab
  // is opened with "noopener", and won't be unset if the opener is closed.
  bool opened_by_another_window_;

  // Set to true while calling out to notify one-off observers (ie non-
  // WebContentsObservers). These observers should not destroy WebContentsImpl
  // while it is on the call stack, as that leads to use-after-frees.
  bool prevent_destruction_ = false;

  bool is_being_destroyed_ = false;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<WebContentsAndroid> web_contents_android_;
#endif

  // Manages the embedder state for browser plugins, if this WebContents is an
  // embedder; NULL otherwise.
  std::unique_ptr<BrowserPluginEmbedder> browser_plugin_embedder_;

  // Manages the guest state for browser plugin, if this WebContents is a guest;
  // NULL otherwise.
  std::unique_ptr<BrowserPluginGuest> browser_plugin_guest_;

  // Helper classes ------------------------------------------------------------

  // Contains information about the WebContents tree structure.
  WebContentsTreeNode node_;

  // Primary FrameTree of this WebContents instance. This WebContents might have
  // additional FrameTrees for features like prerendering and fenced frames,
  // which either might be standalone (prerendering) to nested within a
  // different FrameTree (fenced frame).
  FrameTree primary_frame_tree_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Manages/coordinates multi-process find-in-page requests. Created lazily.
  std::unique_ptr<FindRequestManager> find_request_manager_;

  // Data for loading state ----------------------------------------------------

  // Indicates the process state of the primary main frame's renderer process.
  // If the process is not live due to a crash, this will be reflected by
  // IsCrashed(), though it's possible to not be live while not indicating a
  // crash occurred.
  base::TerminationStatus primary_main_frame_process_status_;
  int primary_main_frame_process_error_code_;

  // The current load state and the URL associated with it.
  net::LoadStateWithParam load_state_;
  std::u16string load_state_host_;
  base::TimeTicks load_info_timestamp_;

  base::TimeTicks loading_last_progress_update_;

  // Default value is set to 100ms between LoadProgressChanged events.
  base::TimeDelta minimum_delay_between_loading_updates_ms_ =
      base::Milliseconds(100);

  // Upload progress, for displaying in the status bar.
  // Set to zero when there is no significant upload happening.
  uint64_t upload_size_;
  uint64_t upload_position_;

  // Tracks that this WebContents needs to unblock requests to the renderer.
  // See ResumeLoadingCreatedWebContents.
  bool is_resume_pending_;

  // Data for current page -----------------------------------------------------

  // The last published theme color.
  std::optional<SkColor> last_sent_theme_color_;

  // The last published background color.
  std::optional<SkColor> last_sent_background_color_;

  // Data for misc internal state ----------------------------------------------

  // When either > 0, the WebContents is currently being captured (e.g.,
  // for screenshots or mirroring); and the underlying RenderWidgetHost
  // should not be told it is hidden. If |visible_capturer_count_| > 0,
  // the underlying Page is set to fully visible. Otherwise, it is set
  // to be hidden but still paint.
  int visible_capturer_count_ = 0;
  int hidden_capturer_count_ = 0;

  // When > 0, |capture_wake_lock_| will be held to prevent display sleep.
  int stay_awake_capturer_count_ = 0;

  // WakeLock held to ensure screen capture keeps the display on. E.g., for
  // presenting through tab capture APIs.
  mojo::Remote<device::mojom::WakeLock> capture_wake_lock_;

  // Remote end of the connection for sending delegated ink points to viz to
  // support the delegated ink trails feature.
  mojo::Remote<gfx::mojom::DelegatedInkPointRenderer>
      delegated_ink_point_renderer_;

  // The visibility of the WebContents. Initialized from
  // |CreateParams::initially_hidden|. Updated from
  // UpdateWebContentsVisibility(), WasShown(), WasHidden(), WasOccluded().
  Visibility visibility_ = Visibility::VISIBLE;

  // Whether there has been a call to UpdateWebContentsVisibility(VISIBLE).
  bool did_first_set_visible_ = false;

  // Indicates whether we should notify about disconnection of this
  // WebContentsImpl. This is used to ensure disconnection notifications only
  // happen if a connection notification has happened and that they happen only
  // once.
  bool notify_disconnection_;

  // Counts the number of outstanding requests to ignore input events. They will
  // not be sent when this is greater than zero.
  int ignore_input_events_count_ = 0;
  uint64_t next_web_input_event_audit_callback_id_ = 0;
  base::flat_map<uint64_t, WebInputEventAuditCallback>
      web_input_event_audit_callbacks_;

  // Pointer to the JavaScript dialog manager, lazily assigned. Used because the
  // delegate of this WebContentsImpl is nulled before its destructor is called.
  raw_ptr<JavaScriptDialogManager, DanglingUntriaged> dialog_manager_;

  // Set to true when there is an active JavaScript dialog showing.
  bool is_showing_javascript_dialog_ = false;

  // Set to true when there is an active "before unload" dialog.  When true,
  // we've forced the throbber to start in Navigate, and we need to remember to
  // turn it off in OnJavaScriptMessageBoxClosed if the navigation is canceled.
  bool is_showing_before_unload_dialog_;

  // Settings that get passed to the renderer process.
  blink::RendererPreferences renderer_preferences_;

  // The time ticks that this WebContents was last made active. The initial
  // value is the WebContents creation time.
  base::TimeTicks last_active_time_ticks_;

  // The time that this WebContents was last made active. The initial value is
  // the WebContents creation time.
  base::Time last_active_time_;

  // The most recent time that this WebContents was interacted with. Currently,
  // this counts:
  // * 'interactive' input events from the user, like mouse clicks and keyboard
  // input but not mouse wheel scrolling
  // * editing commands such as `Paste()`, which are invoked programmatically,
  // presumably in response to user action
  base::TimeTicks last_interaction_time_;

  // See description above setter.
  bool closed_by_user_gesture_;

  // The number of active fullscreen blockers.
  int fullscreen_blocker_count_ = 0;

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

  // When device emulation is enabled, override the size of current and newly
  // created render views/widgets.
  gfx::Size device_emulation_size_;
  gfx::Size view_size_before_emulation_;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Holds information about a current color chooser dialog, if one is visible.
  class ColorChooserHolder;
  std::unique_ptr<ColorChooserHolder> color_chooser_holder_;
#endif

  // All live RenderWidgetHostImpls that are created by this object and may
  // outlive it.
  std::set<raw_ptr<RenderWidgetHostImpl, SetExperimental>> created_widgets_;

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
  base::OnceCallback<void(content::NavigationHandle&)>
      delayed_navigation_handle_callback_;

  // Whether overscroll should be unconditionally disabled.
  bool force_disable_overscroll_content_;

  // Whether the last JavaScript dialog shown was suppressed. Used for testing.
  bool last_dialog_suppressed_;

  mojo::Remote<device::mojom::GeolocationContext> geolocation_context_;

  mojo::AssociatedRemote<blink::mojom::ContextMenuClient> context_menu_client_;

  std::unique_ptr<WakeLockContextHost> wake_lock_context_host_;
  bool enable_wake_locks_ = true;

  // The last set/computed value of WebPreferences for this WebContents, either
  // set directly through SetWebPreferences, or set after recomputing values
  // from ComputeWebPreferences.
  std::unique_ptr<blink::web_pref::WebPreferences> web_preferences_;

  bool updating_web_preferences_ = false;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<NFCHost> nfc_host_;
#endif

  mojo::ReceiverSet<blink::mojom::ColorChooserFactory>
      color_chooser_factory_receivers_;

  std::unique_ptr<ScreenOrientationProvider> screen_orientation_provider_;

  // The accessibility mode for all frames. This is queried when each frame
  // is created, and broadcast to all frames when it changes.
  ui::AXMode accessibility_mode_;

  std::unique_ptr<ui::AXEventRecorder> event_recorder_;

  // Enables ui::kAXModeBasic for the duration of a recording session.
  std::unique_ptr<ScopedAccessibilityMode> recording_mode_;

  // Monitors power levels for audio streams associated with this WebContents.
  AudioStreamMonitor audio_stream_monitor_;

  // Coordinates all the audio streams for this WebContents. Lazily initialized.
  std::optional<ForwardingAudioStreamFactory> audio_stream_factory_;

  size_t bluetooth_connected_device_count_ = 0;
  size_t bluetooth_scanning_sessions_count_ = 0;
  size_t serial_active_frame_count_ = 0;
  size_t hid_active_frame_count_ = 0;
  size_t usb_active_frame_count_ = 0;

  size_t file_system_access_handle_count_ = 0;

  bool has_picture_in_picture_video_ = false;
  bool has_picture_in_picture_document_ = false;

  // Manages media players, CDMs, and power save blockers for media.
  std::unique_ptr<MediaWebContentsObserver> media_web_contents_observer_;

#if BUILDFLAG(ENABLE_PPAPI)
  // Observes pepper playback changes, and notifies MediaSession.
  std::unique_ptr<PepperPlaybackObserver> pepper_playback_observer_;
#endif  // BUILDFLAG(ENABLE_PPAPI)

  // RenderWidgetHostInputEventRouter is uniquely owned by WebContentsImpl in
  // the browser process.
  scoped_refptr<input::RenderWidgetHostInputEventRouter>
      rwh_input_event_router_;

  std::unique_ptr<TouchEmulatorImpl> touch_emulator_;

  // TextInputManager tracks the IME-related state for all the
  // RenderWidgetHostViews on this WebContents. Only exists on the outermost
  // WebContents and is automatically destroyed when a WebContents becomes an
  // inner WebContents by attaching to an outer WebContents. Then the
  // IME-related state for RenderWidgetHosts on the inner WebContents is tracked
  // by the TextInputManager in the outer WebContents.
  std::unique_ptr<TextInputManager> text_input_manager_;

  // Tests can set this to true in order to force this web contents to always
  // return nullptr for the above `text_input_manager_`, effectively blocking
  // IME events from propagating out of the renderer.
  bool suppress_ime_events_for_testing_ = false;

  // Stores the RenderWidgetHost that currently holds a mouse lock or nullptr if
  // there's no RenderWidgetHost holding a lock.
  raw_ptr<RenderWidgetHostImpl, DanglingUntriaged> pointer_lock_widget_ =
      nullptr;

  // Stores the RenderWidgetHost that currently holds a keyboard lock or nullptr
  // if no RenderWidgetHost has the keyboard locked.
  raw_ptr<RenderWidgetHostImpl, DanglingUntriaged> keyboard_lock_widget_ =
      nullptr;

  // Indicates whether the escape key is one of the requested keys to be locked.
  // This information is used to drive the browser UI so the correct exit
  // instructions are displayed to the user in fullscreen mode.
  bool esc_key_locked_ = false;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<service_manager::InterfaceProvider> java_interfaces_;
#endif

  // Whether this WebContents is for content overlay.
  bool is_overlay_content_;

  bool showing_context_menu_;

  int currently_playing_video_count_ = 0;
  base::flat_map<MediaPlayerId, gfx::Size> cached_video_sizes_;

  bool has_persistent_video_ = false;

  bool is_spatial_navigation_disabled_ = false;

  bool stylus_handwriting_enabled_ = false;

#if BUILDFLAG(IS_ANDROID)
  bool long_press_link_select_text_ = false;
#endif

  bool is_currently_audible_ = false;
  bool was_ever_audible_ = false;

  // Helper variable for resolving races in UpdateTargetURL / ClearTargetURL.
  raw_ptr<RenderFrameHost, DanglingUntriaged> frame_that_set_last_target_url_ =
      nullptr;

  // Whether we should override user agent in new tabs.
  bool should_override_user_agent_in_new_tabs_ = false;

  // Used to determine the value of is-user-agent-overriden for renderer
  // initiated navigations.
  NavigationController::UserAgentOverrideOption
      renderer_initiated_user_agent_override_option_ =
          NavigationController::UA_OVERRIDE_INHERIT;

  // Gets notified about changes in viewport fit events.
  std::unique_ptr<SafeAreaInsetsHost> safe_area_insets_host_;

  // Stores a set of frames that are fullscreen.
  // See https://fullscreen.spec.whatwg.org.
  std::set<raw_ptr<RenderFrameHostImpl, SetExperimental>> fullscreen_frames_;

  // Store an ID for the frame that is currently fullscreen, or an invalid ID if
  // there is none.
  GlobalRenderFrameHostId current_fullscreen_frame_id_ =
      GlobalRenderFrameHostId();

  // Whether location bar should be focused by default. This is computed in
  // DidStartNavigation/DidFinishNavigation and only set for an initial
  // navigation triggered by the browser going to about:blank.
  bool should_focus_location_bar_by_default_ = false;

  // Stores the rect of the Windows Control Overlay, which contains system UX
  // affordances (e.g. close), for installed desktop Progress Web Apps (PWAs),
  // if the app specifies the 'window-controls-overlay' DisplayMode in its
  // manifest. This is in frame space coordinates.
  gfx::Rect window_controls_overlay_rect_;

  // Observe native theme for changes to dark mode, forced_colors, preferred
  // color scheme, and preferred contrast. Used to notify the renderer of
  // preferred color scheme and preferred contrast changes.
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};

  base::ScopedObservation<SlowWebPreferenceCache,
                          SlowWebPreferenceCacheObserver>
      slow_web_preference_cache_observation_{this};

  bool using_dark_colors_ = false;
  bool in_forced_colors_ = false;
  ui::NativeTheme::PreferredColorScheme preferred_color_scheme_ =
      ui::NativeTheme::PreferredColorScheme::kLight;
  ui::NativeTheme::PreferredContrast preferred_contrast_ =
      ui::NativeTheme::PreferredContrast::kNoPreference;
  bool prefers_reduced_transparency_ = false;
  bool inverted_colors_ = false;

  // Tracks clients who want to be notified when a JavaScript dialog is
  // dismissed.
  std::unique_ptr<JavaScriptDialogDismissNotifier>
      javascript_dialog_dismiss_notifier_;

  // The max number of loaded frames that have been seen in this WebContents.
  // This number is reset with each main frame navigation.
  size_t max_loaded_frame_count_ = 0;

  // This boolean value is used to keep track of whether we finished the first
  // successful navigation in this WebContents's primary main frame.
  bool first_primary_navigation_completed_ = false;

  // Monitors system screen info changes to notify the renderer.
  std::unique_ptr<ScreenChangeMonitor> screen_change_monitor_;

  // Records the last time we saw a screen orientation change.
  base::TimeTicks last_screen_orientation_change_time_;

  // Indicates how many sources are currently suppressing the unresponsive
  // renderer dialog.
  int suppress_unresponsive_renderer_count_ = 0;

  // Stores all prefetch containers created by `this`.
  std::vector<base::WeakPtr<PrefetchContainer>> prefetch_containers_;

  std::unique_ptr<PrerenderHostRegistry> prerender_host_registry_;

  // Used to ignore multiple back navigation hints in rapid succession. For
  // example, we may get multiple hints due to imprecise mouse movement while
  // the user is trying to move the mouse to the back button.
  base::TimeTicks last_back_navigation_hint_time_ = base::TimeTicks::Min();

  viz::FrameSinkId xr_render_target_;

  // Allows the app in the current WebContents to opt-in to exposing
  // information to apps that capture it.
  blink::mojom::CaptureHandleConfig capture_handle_config_;

  // Background color of the page set by the embedder to be passed to all
  // renderers attached to this WebContents, for use in the main frame.
  // It is used when the page has not loaded enough to know a background
  // color or if the page does not set a background color.
  std::optional<SkColor> page_base_background_color_;

  // Stores WebContents::CreateParams::creator_location.
  base::Location creator_location_;

#if BUILDFLAG(IS_ANDROID)
  // Stores WebContents::CreateParams::java_creator_location.
  base::android::ScopedJavaGlobalRef<jthrowable> java_creator_location_;
#endif  // BUILDFLAG(IS_ANDROID)

  // The options used for WebContents associated with a PictureInPicture window.
  // This value is the parameter given in
  // WebContents::CreateParams::picture_in_picture_options.
  std::optional<blink::mojom::PictureInPictureWindowOptions>
      picture_in_picture_options_;

  // Only set if this WebContents represents a document picture-in-picture
  // window. This points to the WebContents that originally opened this
  // WebContents.
  base::WeakPtr<WebContents> picture_in_picture_opener_;

  VisibleTimeRequestTrigger visible_time_request_trigger_;

  // Counts the number of open scopes that disallow custom cursors in this web
  // contents. Custom cursors are allowed if this is 0.
  int disallow_custom_cursor_scope_count_ = 0;

  base::WeakPtr<FileChooserImpl> active_file_chooser_;

  std::optional<base::Location> ownership_location_;

  // This id is used by Viz to create RenderWidgetHostInputEventRouter per
  // WebContents(concept in browser) to allow grouping CompositorFrameSinks for
  // input event routing with InputVizard.
  const uint32_t compositor_frame_sink_grouping_id_;

  // Indicates if the instance is hosted in a preview window.
  // This will be set in Init() and will be reset in WillActivatePreviewPage().
  bool is_in_preview_mode_ = false;

  // Indicates accessibility had an unrecoverable error.
  bool unrecoverable_accessibility_error_ = false;

  // The network handle bound to the target network, is used to handle the
  // loading requests over a specific network. The network handle is set when
  // WebContents is created and will not change during the life cycle of
  // WebContents.
  net::handles::NetworkHandle target_network_ =
      net::handles::kInvalidNetworkHandle;

  // Whether this contents represents a window initially opened as a new popup.
  bool is_popup_{false};

  // If this window was opened as a new partitioned popin this will be the
  // frame of the opener. This will only have a value if `is_popup_` is true.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  // TODO(crbug.com/340606651): If this is cleared after being set or navigated
  // the popin should be forced to close. Ownership here need to be firmed up.
  base::WeakPtr<RenderFrameHostImpl> partitioned_popin_opener_;

  // Each window can have at most one open partitioned popin, and this will be a
  // pointer to it. If this is set `partitioned_popin_opener_` must be null as
  // no popin can open a popin.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  // TODO(crbug.com/340606651): Ownership here is likely weaker than possible.
  // Given the 1:1 relationship here the opened popin could probably be a
  // unique_ptr cleared via a WebContentsModalDialogManager observer on close.
  base::WeakPtr<WebContents> opened_partitioned_popin_;

  base::WeakPtrFactory<WebContentsImpl> loading_weak_factory_{this};
  base::WeakPtrFactory<WebContentsImpl> weak_factory_{this};
};

// Dangerous methods which should never be made part of the public API, so we
// grant their use only to an explicit friend list (c++ attorney/client idiom).
class CONTENT_EXPORT WebContentsImpl::FriendWrapper {
 public:
  using CreatedCallback = base::RepeatingCallback<void(WebContents*)>;

  FriendWrapper(const FriendWrapper&) = delete;
  FriendWrapper& operator=(const FriendWrapper&) = delete;

 private:
  friend base::CallbackListSubscription RegisterWebContentsCreationCallback(
      base::RepeatingCallback<void(WebContents*)>);

  FriendWrapper();  // Not instantiable.

  // Adds a callback called on creation of each new WebContents.
  static base::CallbackListSubscription AddCreatedCallbackForTesting(
      const CreatedCallback& callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
