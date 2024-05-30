// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DELEGATE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-forward.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_MAC)
#include "components/input/native_web_keyboard_event.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "ui/display/screen.h"
#endif

class GURL;

namespace content {
class ColorChooser;
class FileSelectListener;
class JavaScriptDialogManager;
class Shell;
class ShellPlatformDataAura;
class RenderFrameHost;
class WebContents;

class ShellPlatformDelegate {
 public:
  enum UIControl { BACK_BUTTON, FORWARD_BUTTON, STOP_BUTTON };

  ShellPlatformDelegate();
  virtual ~ShellPlatformDelegate();

  // Helper for one time initialization of application.
  virtual void Initialize(const gfx::Size& default_window_size);

  // Called after creating a Shell instance, with its initial size.
  virtual void CreatePlatformWindow(Shell* shell,
                                    const gfx::Size& initial_size);

  // Notifies of a top-level or nested web contents being created for, or
  // attached to, the Shell.
  virtual void DidCreateOrAttachWebContents(Shell* shell,
                                            WebContents* web_contents);

  // Called from the Shell destructor to let each platform do any necessary
  // cleanup.
  virtual void CleanUp(Shell* shell);

  // Called from the Shell destructor after destroying the last one. This is
  // usually a good time to call Shell::Shutdown().
  virtual void DidCloseLastWindow();

  // Links the WebContents into the newly created window.
  virtual void SetContents(Shell* shell);

  // Resize the web contents in the shell window to the given size.
  virtual void ResizeWebContent(Shell* shell, const gfx::Size& content_size);

  // Enable/disable a button.
  virtual void EnableUIControl(Shell* shell,
                               UIControl control,
                               bool is_enabled);

  // Updates the url in the url bar.
  virtual void SetAddressBarURL(Shell* shell, const GURL& url);

  // Sets whether the spinner is spinning.
  virtual void SetIsLoading(Shell* shell, bool loading);

  // Set the title of shell window
  virtual void SetTitle(Shell* shell, const std::u16string& title);

  // Called when the main frame is created in the renderer process; forwarded
  // from WebContentsObserver. If navigation creates a new main frame, this may
  // occur more than once.
  virtual void MainFrameCreated(Shell* shell);

  // Allows platforms to override the JavascriptDialogManager. By default
  // returns null, which signals that the Shell should use its own instance.
  virtual std::unique_ptr<JavaScriptDialogManager>
  CreateJavaScriptDialogManager(Shell* shell);

  // Requests handling of locking the mouse pointer. This returns true if the
  // request has been handled, otherwise false.
  virtual bool HandlePointerLockRequest(Shell* shell,
                                        WebContents* web_contents,
                                        bool user_gesture,
                                        bool last_unlocked_by_target);

  // Allows platforms to prevent running insecure content. By default returns
  // false, only allowing what Shell allows on its own.
  virtual bool ShouldAllowRunningInsecureContent(Shell* shell);

  // Destroy the Shell. Returns true if the ShellPlatformDelegate did the
  // destruction. Returns false if the Shell should destroy itself.
  virtual bool DestroyShell(Shell* shell);

  // Called when color chooser should open. Returns the opened color chooser.
  // Returns nullptr if we failed to open the color chooser. The color chooser
  // is supported/required for Android or iOS.
  virtual std::unique_ptr<ColorChooser> OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);

  // Called when a file selection is to be done.
  // This function is responsible for calling listener->FileSelected() or
  // listener->FileSelectionCanceled().
  virtual void RunFileChooser(RenderFrameHost* render_frame_host,
                              scoped_refptr<FileSelectListener> listener,
                              const blink::mojom::FileChooserParams& params);

#if !BUILDFLAG(IS_ANDROID)
  // Returns the native window. Valid after calling CreatePlatformWindow().
  virtual gfx::NativeWindow GetNativeWindow(Shell* shell);
#endif

#if BUILDFLAG(IS_MAC)
  // Activate (make key) the native window, and focus the web contents.
  virtual void ActivateContents(Shell* shell, WebContents* contents);

  virtual void DidNavigatePrimaryMainFramePostCommit(Shell* shell,
                                                     WebContents* contents);

  virtual bool HandleKeyboardEvent(Shell* shell,
                                   WebContents* source,
                                   const input::NativeWebKeyboardEvent& event);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void ToggleFullscreenModeForTab(Shell* shell,
                                  WebContents* web_contents,
                                  bool enter_fullscreen);

  bool IsFullscreenForTabOrPending(Shell* shell,
                                   const WebContents* web_contents) const;
#endif

#if BUILDFLAG(IS_ANDROID)
  // Forwarded from WebContentsDelegate.
  void SetOverlayMode(Shell* shell, bool use_overlay_mode);

  // Forwarded from WebContentsObserver.
  void LoadProgressChanged(Shell* shell, double progress);
#endif

 protected:
#if defined(USE_AURA) && !defined(SHELL_USE_TOOLKIT_VIEWS)
  // Helper to avoid duplicating aura's ShellPlatformDelegate in web tests. If
  // this hack gets expanded to become more expansive then we should just
  // duplicate the aura ShellPlatformDelegate code to the web test code impl in
  // WebTestShellPlatformDelegate.
  ShellPlatformDataAura* GetShellPlatformDataAura();
#endif

 private:
#if BUILDFLAG(IS_APPLE)
  std::unique_ptr<display::ScopedNativeScreen> screen_;
#endif
  // Data held for each Shell instance, since there is one ShellPlatformDelegate
  // for the whole browser process (shared across Shells). This is defined for
  // each platform implementation.
  struct ShellData;
  // Holds an instance of ShellData for each Shell.
  base::flat_map<Shell*, ShellData> shell_data_map_;

  // Data held in ShellPlatformDelegate that is shared between all Shells. This
  // is created in Initialize(), and is defined for each platform
  // implementation.
  struct PlatformData;
  std::unique_ptr<PlatformData> platform_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DELEGATE_H_
