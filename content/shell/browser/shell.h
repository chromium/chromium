// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_SHELL_BROWSER_SHELL_H_
#define CONTENT_SHELL_BROWSER_SHELL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/shell/browser/shell_platform_delegate.h"
#include "ipc/ipc_channel.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace content {
class BrowserContext;
class JavaScriptDialogManager;
class ShellDevToolsFrontend;
class SiteInstance;
class WebContents;

// This represents one window of the Content Shell, i.e. all the UI including
// buttons and url bar, as well as the web content area.
class Shell : public WebContentsDelegate, public WebContentsObserver {
 public:
  ~Shell() override;

  void LoadURL(const GURL& url);
  void LoadURLForFrame(const GURL& url,
                       const std::string& frame_name,
                       ui::PageTransition);
  void LoadDataWithBaseURL(const GURL& url,
                           const std::string& data,
                           const GURL& base_url);

#if BUILDFLAG(IS_ANDROID)
  // Android-only path to allow loading long data strings.
  void LoadDataAsStringWithBaseURL(const GURL& url,
                                   const std::string& data,
                                   const GURL& base_url);
#endif
  void GoBackOrForward(int offset);
  void Reload();
  void ReloadBypassingCache();
  void Stop();
  void UpdateNavigationControls(bool should_show_loading_ui);
  void Close();
  void ShowDevTools();
  void CloseDevTools();
  // Resizes the web content view to the given dimensions.
  void ResizeWebContentForTests(const gfx::Size& content_size);

  // Do one-time initialization at application startup. This must be matched
  // with a Shell::Shutdown() at application termination, where |platform|
  // will be released.
  static void Initialize(std::unique_ptr<ShellPlatformDelegate> platform);

  // Closes all windows, pumps teardown tasks and signal the main message loop
  // to quit.
  static void Shutdown();  // Idempotent, can be called twice.

  static Shell* CreateNewWindow(
      BrowserContext* browser_context,
      const GURL& url,
      const scoped_refptr<SiteInstance>& site_instance,
      const gfx::Size& initial_size);

  // Returns the Shell object corresponding to the given WebContents.
  static Shell* FromWebContents(WebContents* web_contents);

  // Returns the currently open windows.
  static std::vector<Shell*>& windows() { return windows_; }

  // Stores the supplied |quit_closure|, to be run when the last Shell instance
  // is destroyed.
  static void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure);

  // Used by the WebTestControlHost to stop the message loop before closing all
  // windows, for specific tests. Has no effect if the loop is already quitting.
  static void QuitMainMessageLoopForTesting();

  // Used for content_browsertests. Called once.
  static void SetShellCreatedCallback(
      base::OnceCallback<void(Shell*)> shell_created_callback);

  static bool ShouldHideToolbar();

  WebContents* web_contents() const { return web_contents_.get(); }

#if !BUILDFLAG(IS_ANDROID)
  gfx::NativeWindow window();
#endif

#if BUILDFLAG(IS_MAC)
  // Public to be called by an ObjC bridge object.
  void ActionPerformed(int control);
  void URLEntered(const std::string& url_string);
#endif

  // WebContentsDelegate
  WebContents* OpenURLFromTab(WebContents* source,
                              const OpenURLParams& params) override;
  void AddNewContents(WebContents* source,
                      std::unique_ptr<WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture,
                      bool* was_blocked) override;
  void LoadingStateChanged(WebContents* source,
                           bool should_show_loading_ui) override;
#if BUILDFLAG(IS_ANDROID)
  void SetOverlayMode(bool use_overlay_mode) override;
#endif
  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const WebContents* web_contents) override;
#if !BUILDFLAG(IS_ANDROID)
  void RegisterProtocolHandler(RenderFrameHost* requesting_frame,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override;
#endif
  void RequestToLockMouse(WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void CloseContents(WebContents* source) override;
  bool CanOverscrollContent() override;
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override;
  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override;
#if BUILDFLAG(IS_MAC)
  bool HandleKeyboardEvent(WebContents* source,
                           const NativeWebKeyboardEvent& event) override;
#endif
  bool DidAddMessageToConsole(WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void PortalWebContentsCreated(WebContents* portal_web_contents) override;
  void RendererUnresponsive(
      WebContents* source,
      RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void ActivateContents(WebContents* contents) override;
  bool IsBackForwardCacheSupported() override;
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override;
  std::unique_ptr<WebContents> ActivatePortalWebContents(
      WebContents* predecessor_contents,
      std::unique_ptr<WebContents> portal_contents) override;
  void UpdateInspectedWebContentsIfNecessary(
      WebContents* old_contents,
      WebContents* new_contents,
      base::OnceCallback<void()> callback) override;
  bool ShouldAllowRunningInsecureContent(WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  PictureInPictureResult EnterPictureInPicture(
      WebContents* web_contents) override;
  bool ShouldResumeRequestsForCreatedWindow() override;
  void SetContentsBounds(WebContents* source, const gfx::Rect& bounds) override;

  static gfx::Size GetShellDefaultSize();

  void set_delay_popup_contents_delegate_for_testing(bool delay) {
    delay_popup_contents_delegate_for_testing_ = delay;
  }

 private:
  class DevToolsWebContentsObserver;

  Shell(std::unique_ptr<WebContents> web_contents, bool should_set_delegate);

  // Helper to create a new Shell given a newly created WebContents.
  static Shell* CreateShell(std::unique_ptr<WebContents> web_contents,
                            const gfx::Size& initial_size,
                            bool should_set_delegate);

  // Adjust the size when Blink sends 0 for width and/or height.
  // This happens when Blink requests a default-sized window.
  static gfx::Size AdjustWindowSize(const gfx::Size& initial_size);

  // Helper method for the two public LoadData methods.
  void LoadDataWithBaseURLInternal(const GURL& url,
                                   const std::string& data,
                                   const GURL& base_url,
                                   bool load_as_string);

  gfx::NativeView GetContentView();

  void ToggleFullscreenModeForTab(WebContents* web_contents,
                                  bool enter_fullscreen);
  // WebContentsObserver
#if BUILDFLAG(IS_ANDROID)
  void LoadProgressChanged(double progress) override;
#endif
  void TitleWasSet(NavigationEntry* entry) override;
  void RenderFrameCreated(RenderFrameHost* frame_host) override;
#if BUILDFLAG(IS_MAC)
  void PrimaryPageChanged(Page& page) override;
#endif

  std::unique_ptr<JavaScriptDialogManager> dialog_manager_;

  std::unique_ptr<WebContents> web_contents_;

  base::WeakPtr<ShellDevToolsFrontend> devtools_frontend_;

  bool is_fullscreen_ = false;

  gfx::Size content_size_;

  bool delay_popup_contents_delegate_for_testing_ = false;

  // A container of all the open windows. We use a vector so we can keep track
  // of ordering.
  static std::vector<Shell*> windows_;

  static base::OnceCallback<void(Shell*)> shell_created_callback_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_H_
