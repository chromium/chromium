// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace extensions {

// app_shell's NativeAppWindow implementation.
class ShellNativeAppWindow : public NativeAppWindow {
 public:
  ShellNativeAppWindow(AppWindow* app_window,
                       const AppWindow::CreateParams& params);

  ShellNativeAppWindow(const ShellNativeAppWindow&) = delete;
  ShellNativeAppWindow& operator=(const ShellNativeAppWindow&) = delete;

  ~ShellNativeAppWindow() override;

  AppWindow* app_window() const { return app_window_; }

  // ui::BaseWindow overrides:
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  void ShowInactive() override;
  void Close() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void FlashFrame(bool flash) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;

  // web_modal::ModalDialogHost overrides:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // web_modal::WebContentsModalDialogHost overrides:
  gfx::Size GetMaximumDialogSize() override;

  // NativeAppWindow overrides:
  void SetFullscreen(int fullscreen_types) override;
  bool IsFullscreenOrPending() const override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions) override;
  SkRegion* GetDraggableRegion() override;
  void UpdateShape(std::unique_ptr<ShapeRects> rects) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  bool IsFrameless() const override;
  bool HasFrameColor() const override;
  SkColor ActiveFrameColor() const override;
  SkColor InactiveFrameColor() const override;
  gfx::Insets GetFrameInsets() const override;
  gfx::RoundedCornersF GetWindowRadii() const override;
  void SetContentSizeConstraints(const gfx::Size& min_size,
                                 const gfx::Size& max_size) override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool CanHaveAlphaEnabled() const override;
  void SetActivateOnPointer(bool activate_on_pointer) override;

 private:
  raw_ptr<AppWindow, DanglingUntriaged> app_window_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_H_
