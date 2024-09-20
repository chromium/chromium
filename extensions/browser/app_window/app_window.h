// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/gfx/image/image.h"

class GURL;
class SkRegion;

namespace base {
class Value;
}  // namespace base

namespace gfx {
class Rect;
class RoundedCornersF;
}  // namespace gfx

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {

class AppDelegate;
class AppWebContentsHelper;
class Extension;
class PlatformAppBrowserTest;

// Manages the web contents for app windows. The implementation for this
// class should create and maintain the WebContents for the window, and handle
// any message passing between the web contents and the extension system or
// native window.
class AppWindowContents {
 public:
  AppWindowContents() {}

  AppWindowContents(const AppWindowContents&) = delete;
  AppWindowContents& operator=(const AppWindowContents&) = delete;

  virtual ~AppWindowContents() {}

  // Called to initialize the WebContents, before the app window is created.
  virtual void Initialize(content::BrowserContext* context,
                          content::RenderFrameHost* creator_frame,
                          const GURL& url) = 0;

  // Called to load the contents, after the app window is created.
  virtual void LoadContents(int32_t creator_process_id) = 0;

  // Called when the native window changes.
  virtual void NativeWindowChanged(NativeAppWindow* native_app_window) = 0;

  // Called when the native window closes. |send_onclosed| is flag to indicate
  // whether the OnClosed event should be sent. It is true except when the
  // native window is closed before AppWindowCreateFunction responds.
  virtual void NativeWindowClosed(bool send_onclosed) = 0;

  virtual content::WebContents* GetWebContents() const = 0;

  virtual extensions::WindowController* GetWindowController() const = 0;
};

// AppWindow is the type of window used by platform apps. App windows
// have a WebContents but none of the chrome of normal browser windows.
class AppWindow : public content::WebContentsDelegate,
                  public content::WebContentsObserver,
                  public web_modal::WebContentsModalDialogManagerDelegate,
                  public ExtensionFunctionDispatcher::Delegate,
                  public ExtensionRegistryObserver {
 public:
  // Enum values should not be changed since they are used by UMA.
  enum WindowType {
    WINDOW_TYPE_DEFAULT = 0,  // Default app window.
    DEPRECATED_WINDOW_TYPE_PANEL = 1,
    WINDOW_TYPE_COUNT = 2,
  };

  enum Frame {
    FRAME_CHROME,  // Chrome-style window frame.
    FRAME_NONE,    // Frameless window.
  };

  enum FullscreenType {
    // Not fullscreen.
    FULLSCREEN_TYPE_NONE = 0,

    // Fullscreen entered by the app.window api.
    FULLSCREEN_TYPE_WINDOW_API = 1 << 0,

    // Fullscreen entered by HTML requestFullscreen().
    FULLSCREEN_TYPE_HTML_API = 1 << 1,

    // Fullscreen entered by the OS. ChromeOS uses this type of fullscreen to
    // enter immersive fullscreen when the user hits the <F4> key.
    FULLSCREEN_TYPE_OS = 1 << 2,

    // Fullscreen mode that could not be exited by the user. ChromeOS uses
    // this type of fullscreen to run an app in kiosk mode.
    FULLSCREEN_TYPE_FORCED = 1 << 3,
  };

  using ShapeRects = std::vector<gfx::Rect>;

  struct BoundsSpecification {
    // INT_MIN represents an unspecified position component.
    static const int kUnspecifiedPosition;

    BoundsSpecification();
    ~BoundsSpecification();

    // INT_MIN designates 'unspecified' for the position components, and 0
    // designates 'unspecified' for the size components. When unspecified,
    // they should be replaced with a default value.
    gfx::Rect bounds;

    gfx::Size minimum_size;
    gfx::Size maximum_size;

    // Reset the bounds fields to their 'unspecified' values. The minimum and
    // maximum size constraints remain unchanged.
    void ResetBounds();
  };

  struct CreateParams {
    CreateParams();
    CreateParams(const CreateParams& other);
    ~CreateParams();

    WindowType window_type;
    Frame frame;

    bool has_frame_color;
    SkColor active_frame_color;
    SkColor inactive_frame_color;
    bool alpha_enabled;
    bool is_ime_window;

    // The initial content/inner bounds specification (excluding any window
    // decorations).
    BoundsSpecification content_spec;

    // The initial window/outer bounds specification (including window
    // decorations).
    BoundsSpecification window_spec;

    std::string window_key;

    // The process ID of the process that requested the create.
    int32_t creator_process_id;

    // Initial state of the window.
    ui::mojom::WindowShowState state;

    // If true, don't show the window after creation.
    bool hidden;

    // If true, the window will be resizable by the user. Defaults to true.
    bool resizable;

    // If true, the window will be focused on creation. Defaults to true.
    bool focused;

    // If true, the window will stay on top of other windows that are not
    // configured to be always on top. Defaults to false.
    bool always_on_top;

    // If true, the window will be visible on all workspaces. Defaults to false.
    bool visible_on_all_workspaces;

    // Whether the app window should be shown on the lock screen.
    // Chrome OS only.
    bool show_on_lock_screen;

    // If true, the window will have its own shelf icon. Otherwise the window
    // will be grouped in the shelf with other windows that are associated with
    // the app. Defaults to false.
    bool show_in_shelf;

    // Icon URL to be used for setting the window icon.
    GURL window_icon_url;

    // The API enables developers to specify content or window bounds. This
    // function combines them into a single, constrained window size.
    gfx::Rect GetInitialWindowBounds(
        const gfx::Insets& frame_insets,
        const gfx::RoundedCornersF& window_radii) const;

    // The API enables developers to specify content or window size constraints.
    // These functions combine them so that we can work with one set of
    // constraints.
    gfx::Size GetContentMinimumSize(const gfx::Insets& frame_insets) const;
    gfx::Size GetContentMaximumSize(const gfx::Insets& frame_insets) const;
    gfx::Size GetWindowMinimumSize(
        const gfx::Insets& frame_insets,
        const gfx::RoundedCornersF& window_radii) const;
    gfx::Size GetWindowMaximumSize(
        const gfx::Insets& frame_insets,
        const gfx::RoundedCornersF& window_radii) const;
  };

  // Convert draggable regions in raw format to SkRegion format. Caller is
  // responsible for deleting the returned SkRegion instance.
  static SkRegion* RawDraggableRegionsToSkRegion(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions);

  // The constructor and Init methods are public for constructing a AppWindow
  // with a non-standard render interface (e.g.
  // lock_screen_apps::StateController, ChromeAppWindowClient). Normally
  // AppWindow::Create should be used.
  AppWindow(content::BrowserContext* context,
            std::unique_ptr<AppDelegate> app_delegate,
            const Extension* extension);

  AppWindow(const AppWindow&) = delete;
  AppWindow& operator=(const AppWindow&) = delete;

  // Initializes the render interface, web contents, and native window.
  void Init(const GURL& url,
            std::unique_ptr<AppWindowContents> app_window_contents,
            content::RenderFrameHost* creator_frame,
            const CreateParams& params);

  const std::string& window_key() const { return window_key_; }
  SessionID session_id() const { return session_id_; }
  const ExtensionId& extension_id() const { return extension_id_; }
  content::WebContents* web_contents() const;
  WindowType window_type() const { return window_type_; }
  content::BrowserContext* browser_context() const { return browser_context_; }
  const gfx::Image& custom_app_icon() const { return custom_app_icon_; }
  const GURL& app_icon_url() const { return app_icon_url_; }
  const GURL& initial_url() const { return initial_url_; }
  bool is_hidden() const { return is_hidden_; }

  // Calls to this should always be guarded by a nullptr check as this can
  // return nullptr if the extension is no longer installed.
  const Extension* GetExtension() const;

  NativeAppWindow* GetBaseWindow();
  gfx::NativeWindow GetNativeWindow();

  // Returns the bounds that should be reported to the renderer.
  gfx::Rect GetClientBounds() const;

  // NativeAppWindows should call this to determine what the window's title
  // is on startup and from within UpdateWindowTitle().
  std::u16string GetTitle() const;

  // |callback| will be called when the first navigation was completed or window
  // is closed before that. |did_finish| argument of the |callback| is set to
  // true for the former case and false for the latter.
  using DidFinishFirstNavigationCallback =
      base::OnceCallback<void(bool did_finish)>;
  void AddOnDidFinishFirstNavigationCallback(
      DidFinishFirstNavigationCallback callback);

  // Called when first navigation was completed.
  void OnDidFinishFirstNavigation();

  // Call to notify ShellRegistry and delete the window. Subclasses should
  // invoke this method instead of using "delete this".
  void OnNativeClose();

  // Should be called by native implementations when the window size, position,
  // or minimized/maximized state has changed.
  void OnNativeWindowChanged();

  // Should be called by native implementations when the window is activated.
  void OnNativeWindowActivated();

  // Specifies a url for the launcher icon.
  void SetAppIconUrl(const GURL& icon_url);

  // Sets the window shape. Passing a nullptr |rects| sets the default shape.
  void UpdateShape(std::unique_ptr<ShapeRects> rects);

  // Notify hat an app window is ready and can resume resource requests.
  void AppWindowReady();

  // Updates the app image to |image|. Called internally from the image loader
  // callback.
  void UpdateAppIcon(const gfx::Image& image);

  // Enable or disable fullscreen mode. |type| specifies which type of
  // fullscreen mode to change (note that disabling one type of fullscreen may
  // not exit fullscreen mode because a window may have a different type of
  // fullscreen enabled). If |type| is not FORCED, checks that the extension has
  // the required permission.
  void SetFullscreen(FullscreenType type, bool enable);

  // Returns true if the app window is in a fullscreen state.
  bool IsFullscreen() const;

  // Returns true if the app window is in a forced fullscreen state (one that
  // cannot be exited by the user).
  bool IsForcedFullscreen() const;

  // Returns true if the app window is in a fullscreen state entered from an
  // HTML API request.
  bool IsHtmlApiFullscreen() const;

  bool IsOsFullscreen() const;

  // Transitions window into fullscreen, maximized, minimized or restores based
  // on chrome.app.window API.
  void Fullscreen();
  void Maximize();
  void Minimize();
  void Restore();

  // Transitions to OS fullscreen. See FULLSCREEN_TYPE_OS for more details.
  void OSFullscreen();

  // Transitions to forced fullscreen. See FULLSCREEN_TYPE_FORCED for more
  // details.
  void ForcedFullscreen();

  // Set the minimum and maximum size of the content bounds.
  void SetContentSizeConstraints(const gfx::Size& min_size,
                                 const gfx::Size& max_size);

  enum ShowType { SHOW_ACTIVE, SHOW_INACTIVE };

  // Shows the window if its contents have been painted; otherwise flags the
  // window to be shown as soon as its contents are painted for the first time.
  void Show(ShowType show_type);

  // Hides the window. If the window was previously flagged to be shown on
  // first paint, it will be unflagged.
  void Hide();

  AppWindowContents* app_window_contents_for_test() {
    return app_window_contents_.get();
  }

  int fullscreen_types_for_test() { return fullscreen_types_; }

  // Set whether the window should stay above other windows which are not
  // configured to be always-on-top.
  void SetAlwaysOnTop(bool always_on_top);

  // Whether the always-on-top property has been set by the chrome.app.window
  // API. Note that the actual value of this property in the native app window
  // may be false if the bit is silently switched off for security reasons.
  bool IsAlwaysOnTop() const;

  // Restores the always-on-top property according to |cached_always_on_top_|.
  void RestoreAlwaysOnTop();

  // Retrieve the current state of the app window as a dictionary, to pass to
  // the renderer.
  void GetSerializedState(base::Value::Dict* properties) const;

  // Whether the app window wants to be alpha enabled.
  bool requested_alpha_enabled() const { return requested_alpha_enabled_; }

  // Whether the app window is created by IME extensions.
  // TODO(bshe): rename to hide_app_window_in_launcher if it is not used
  // anywhere other than app_window_launcher_controller after M45. Otherwise,
  // remove this TODO.
  bool is_ime_window() const { return is_ime_window_; }

  bool show_on_lock_screen() const { return show_on_lock_screen_; }

  bool show_in_shelf() const { return show_in_shelf_; }

  AppDelegate* app_delegate() { return app_delegate_.get(); }

  void SetAppWindowContentsForTesting(
      std::unique_ptr<AppWindowContents> contents) {
    app_window_contents_ = std::move(contents);
  }

  void SetNativeAppWindowForTesting(
      std::unique_ptr<NativeAppWindow> native_app_window) {
    native_app_window_ = std::move(native_app_window);
  }

  void SetOnDraggableRegionsChangedForTesting(base::OnceClosure callback) {
    on_update_draggable_regions_callback_for_testing_ = std::move(callback);
  }

  bool DidFinishFirstNavigation() { return did_finish_first_navigation_; }

 protected:
  ~AppWindow() override;

 private:
  // PlatformAppBrowserTest needs access to web_contents()
  friend class PlatformAppBrowserTest;

  // content::WebContentsDelegate implementation.
  void ActivateContents(content::WebContents* contents) override;
  void CloseContents(content::WebContents* contents) override;
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* source) override;
  bool IsFullscreenForTabOrPending(const content::WebContents* source) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* source) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void RequestPointerLock(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  bool ShouldShowStaleContentOnEviction(content::WebContents* source) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& draggable_regions,
      content::WebContents* contents) override;

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* frame_host) override;

  // ExtensionFunctionDispatcher::Delegate implementation.
  WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // web_modal::WebContentsModalDialogManagerDelegate implementation.
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  void ToggleFullscreenModeForTab(content::WebContents* source,
                                  bool enter_fullscreen);

  // Saves the window geometry/position/screen bounds.
  void SaveWindowPosition();

  // Helper method to adjust the cached bounds so that we can make sure it can
  // be visible on the screen. See http://crbug.com/145752.
  void AdjustBoundsToBeVisibleOnScreen(const gfx::Rect& cached_bounds,
                                       const gfx::Rect& cached_screen_bounds,
                                       const gfx::Rect& current_screen_bounds,
                                       const gfx::Size& minimum_size,
                                       gfx::Rect* bounds) const;

  // Loads the appropriate default or cached window bounds. Returns a new
  // CreateParams that should be used to create the window.
  CreateParams LoadDefaults(CreateParams params) const;

  // Set the fullscreen state in the native app window.
  void SetNativeWindowFullscreen();

  // Returns true if there is any overlap between the window and the taskbar
  // (Windows only).
  bool IntersectsWithTaskbar() const;

  // Update the always-on-top bit in the native app window.
  void UpdateNativeAlwaysOnTop();

  // Sends the onWindowShown event to the app if the window has been shown. Only
  // has an effect in tests.
  void SendOnWindowShownIfShown();

  // web_modal::WebContentsModalDialogManagerDelegate implementation.
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Starts custom app icon download. To avoid race condition with loading app
  // itself it is started in case |app_icon_url_| is set and app window is
  // ready.
  void StartAppIconDownload();

  // Callback from web_contents()->DownloadFavicon.
  void DidDownloadFavicon(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& original_bitmap_sizes);

  // The browser context with which this window is associated. AppWindow does
  // not own this object.
  raw_ptr<content::BrowserContext> browser_context_;

  const ExtensionId extension_id_;

  // Identifier that is used when saving and restoring geometry for this
  // window.
  std::string window_key_;

  const SessionID session_id_;
  WindowType window_type_ = WINDOW_TYPE_DEFAULT;

  // Custom icon shown in the task bar or in Chrome OS shelf.
  gfx::Image custom_app_icon_;

  // Icon URL to be used for setting the app icon. If not empty, app_icon_ will
  // be fetched and set using this URL.
  GURL app_icon_url_;

  // These callbacks are called when the navigation is finished on both browser
  // and renderer sides.
  std::vector<DidFinishFirstNavigationCallback>
      on_did_finish_first_navigation_callbacks_;

  // Whether the first navigation was completed in both browser and renderer
  // processes.
  bool did_finish_first_navigation_ = false;

  std::unique_ptr<NativeAppWindow> native_app_window_;
  std::unique_ptr<AppWindowContents> app_window_contents_;
  std::unique_ptr<AppDelegate> app_delegate_;
  std::unique_ptr<AppWebContentsHelper> helper_;

  // The initial url this AppWindow was navigated to.
  GURL initial_url_;

  // Bit field of FullscreenType.
  int fullscreen_types_ = FULLSCREEN_TYPE_NONE;

  // Whether the window has been shown or not.
  bool has_been_shown_ = false;

  // Whether the window is hidden or not. Hidden in this context means actively
  // by the chrome.app.window API, not in an operating system context. For
  // example windows which are minimized are not hidden, and windows which are
  // part of a hidden app on OS X are not hidden. Windows which were created
  // with the |hidden| flag set to true, or which have been programmatically
  // hidden, are considered hidden.
  bool is_hidden_ = false;

  // Cache the desired value of the always-on-top property. When windows enter
  // fullscreen or overlap the Windows taskbar, this property will be
  // automatically and silently switched off for security reasons. It is
  // reinstated when the window exits fullscreen and moves away from the
  // taskbar.
  bool cached_always_on_top_ = false;

  // Whether |alpha_enabled| was set in the CreateParams.
  bool requested_alpha_enabled_ = false;

  // Whether |is_ime_window| was set in the CreateParams.
  bool is_ime_window_ = false;

  // Whether |show_on_lock_screen| was set in the CreateParams.
  bool show_on_lock_screen_ = false;

  // Whether |show_in_shelf| was set in the CreateParams.
  bool show_in_shelf_ = false;

  // Whether the app window is loaded and ready. It is used to resolve the
  // race condition of loading custom app icon and app content simultaneously.
  bool window_ready_ = false;

  // Allows tests to wait for draggable regions to be sent from the renderer.
  base::OnceClosure on_update_draggable_regions_callback_for_testing_;

  base::WeakPtrFactory<AppWindow> image_loader_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_H_
