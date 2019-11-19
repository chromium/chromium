// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_native_app_window.h"

#include "extensions/shell/browser/desktop_controller.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

ShellNativeAppWindow::ShellNativeAppWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& params)
    : app_window_(app_window) {
}

ShellNativeAppWindow::~ShellNativeAppWindow() {
}

bool ShellNativeAppWindow::IsMaximized() const {
  return false;
}

bool ShellNativeAppWindow::IsMinimized() const {
  return false;
}

bool ShellNativeAppWindow::IsFullscreen() const {
  // The window in app_shell is considered a "restored" window that happens to
  // fill the display. This avoids special handling of fullscreen or maximized
  // windows that app_shell doesn't need.
  return false;
}

gfx::Rect ShellNativeAppWindow::GetRestoredBounds() const {
  // app_shell windows cannot be maximized, so the current bounds are the
  // restored bounds.
  return GetBounds();
}

ui::WindowShowState ShellNativeAppWindow::GetRestoredState() const {
  return ui::SHOW_STATE_NORMAL;
}

void ShellNativeAppWindow::ShowInactive() {
  // app_shell does not differentiate between active and inactive windows.
  Show();
}

void ShellNativeAppWindow::Close() {
  app_window_->OnNativeClose();
}

void ShellNativeAppWindow::Maximize() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::Minimize() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::Restore() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::FlashFrame(bool flash) {
  NOTIMPLEMENTED();
}

ui::ZOrderLevel ShellNativeAppWindow::GetZOrderLevel() const {
  return ui::ZOrderLevel::kNormal;
}

void ShellNativeAppWindow::SetZOrderLevel(ui::ZOrderLevel level) {
  NOTIMPLEMENTED();
}

gfx::NativeView ShellNativeAppWindow::GetHostView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::Point ShellNativeAppWindow::GetDialogPosition(const gfx::Size& size) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

void ShellNativeAppWindow::AddObserver(
      web_modal::ModalDialogHostObserver* observer) {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) {
  NOTIMPLEMENTED();
}

gfx::Size ShellNativeAppWindow::GetMaximumDialogSize() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

void ShellNativeAppWindow::SetFullscreen(int fullscreen_types) {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindow::IsFullscreenOrPending() const {
  // See comment in IsFullscreen().
  return false;
}

void ShellNativeAppWindow::UpdateWindowIcon() {
  // No icon to update.
}

void ShellNativeAppWindow::UpdateWindowTitle() {
  // No window title to update.
}

void ShellNativeAppWindow::UpdateDraggableRegions(
    const std::vector<DraggableRegion>& regions) {
  NOTIMPLEMENTED();
}

SkRegion* ShellNativeAppWindow::GetDraggableRegion() {
  NOTIMPLEMENTED();
  return NULL;
}

void ShellNativeAppWindow::UpdateShape(std::unique_ptr<ShapeRects> rects) {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindow::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // No special handling. The WebContents will handle it.
  return false;
}

bool ShellNativeAppWindow::IsFrameless() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellNativeAppWindow::HasFrameColor() const {
  return false;
}

SkColor ShellNativeAppWindow::ActiveFrameColor() const {
  return SkColor();
}

SkColor ShellNativeAppWindow::InactiveFrameColor() const {
  return SkColor();
}

gfx::Insets ShellNativeAppWindow::GetFrameInsets() const {
  return gfx::Insets();
}

void ShellNativeAppWindow::SetContentSizeConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size) {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::SetVisibleOnAllWorkspaces(bool always_visible) {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindow::CanHaveAlphaEnabled() const {
  // No background to display if the window was transparent.
  return false;
}

void ShellNativeAppWindow::SetActivateOnPointer(bool activate_on_pointer) {
  NOTIMPLEMENTED();
}

}  // namespace extensions
