// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/components/native_app_window/native_app_window_views.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/draggable_region.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace native_app_window {

NativeAppWindowViews::NativeAppWindowViews() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void NativeAppWindowViews::Init(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& create_params) {
  app_window_ = app_window;
  frameless_ = create_params.frame == extensions::AppWindow::FRAME_NONE;
  resizable_ = create_params.resizable;
  size_constraints_.set_minimum_size(
      create_params.GetContentMinimumSize(gfx::Insets()));
  size_constraints_.set_maximum_size(
      create_params.GetContentMaximumSize(gfx::Insets()));
  Observe(app_window_->web_contents());

  widget_ = new views::Widget;
  widget_->AddObserver(this);
  InitializeWindow(app_window, create_params);

  OnViewWasResized();
}

NativeAppWindowViews::~NativeAppWindowViews() {
  web_view_->SetWebContents(nullptr);
}

void NativeAppWindowViews::OnCanHaveAlphaEnabledChanged() {
  app_window_->OnNativeWindowChanged();
}

void NativeAppWindowViews::InitializeWindow(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& create_params) {
  // Stub implementation. See also ChromeNativeAppWindowViews.
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  if (create_params.always_on_top)
    init_params.z_order = ui::ZOrderLevel::kFloatingWindow;
  widget_->Init(std::move(init_params));
  widget_->CenterWindow(
      create_params.GetInitialWindowBounds(gfx::Insets()).size());
}

// ui::BaseWindow implementation.

bool NativeAppWindowViews::IsActive() const {
  return widget_->IsActive();
}

bool NativeAppWindowViews::IsMaximized() const {
  return widget_->IsMaximized();
}

bool NativeAppWindowViews::IsMinimized() const {
  return widget_->IsMinimized();
}

bool NativeAppWindowViews::IsFullscreen() const {
  return widget_->IsFullscreen();
}

gfx::NativeWindow NativeAppWindowViews::GetNativeWindow() const {
  return widget_->GetNativeWindow();
}

gfx::Rect NativeAppWindowViews::GetRestoredBounds() const {
  return widget_->GetRestoredBounds();
}

ui::WindowShowState NativeAppWindowViews::GetRestoredState() const {
  // Stub implementation. See also ChromeNativeAppWindowViews.
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect NativeAppWindowViews::GetBounds() const {
  return widget_->GetWindowBoundsInScreen();
}

void NativeAppWindowViews::Show() {
  if (widget_->IsVisible()) {
    widget_->Activate();
    return;
  }
  widget_->Show();
}

void NativeAppWindowViews::ShowInactive() {
  if (widget_->IsVisible())
    return;

  widget_->ShowInactive();
}

void NativeAppWindowViews::Hide() {
  widget_->Hide();
}

bool NativeAppWindowViews::IsVisible() const {
  return widget_->IsVisible();
}

void NativeAppWindowViews::Close() {
  widget_->Close();
}

void NativeAppWindowViews::Activate() {
  widget_->Activate();
}

void NativeAppWindowViews::Deactivate() {
  widget_->Deactivate();
}

void NativeAppWindowViews::Maximize() {
  widget_->Maximize();
}

void NativeAppWindowViews::Minimize() {
  widget_->Minimize();
}

void NativeAppWindowViews::Restore() {
  widget_->Restore();
}

void NativeAppWindowViews::SetBounds(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);
}

void NativeAppWindowViews::FlashFrame(bool flash) {
  widget_->FlashFrame(flash);
}

ui::ZOrderLevel NativeAppWindowViews::GetZOrderLevel() const {
  // Stub implementation. See also ChromeNativeAppWindowViews.
  return widget_->GetZOrderLevel();
}

void NativeAppWindowViews::SetZOrderLevel(ui::ZOrderLevel order) {
  widget_->SetZOrderLevel(order);
}

gfx::NativeView NativeAppWindowViews::GetHostView() const {
  return widget_->GetNativeView();
}

gfx::Point NativeAppWindowViews::GetDialogPosition(const gfx::Size& size) {
  gfx::Size app_window_size = widget_->GetWindowBoundsInScreen().size();
  return gfx::Point((app_window_size.width() - size.width()) / 2,
                    (app_window_size.height() - size.height()) / 2);
}

gfx::Size NativeAppWindowViews::GetMaximumDialogSize() {
  return widget_->GetWindowBoundsInScreen().size();
}

void NativeAppWindowViews::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.AddObserver(observer);
}
void NativeAppWindowViews::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void NativeAppWindowViews::OnViewWasResized() {
  for (auto& observer : observer_list_)
    observer.OnPositionRequiresUpdate();
}

// WidgetDelegate implementation.

void NativeAppWindowViews::OnWidgetMove() {
  app_window_->OnNativeWindowChanged();
}

views::View* NativeAppWindowViews::GetInitiallyFocusedView() {
  return web_view_;
}

bool NativeAppWindowViews::CanResize() const {
  return resizable_ && !size_constraints_.HasFixedSize() &&
         !WidgetHasHitTestMask();
}

bool NativeAppWindowViews::CanMaximize() const {
  return resizable_ && !size_constraints_.HasMaximumSize() &&
         !WidgetHasHitTestMask();
}

bool NativeAppWindowViews::CanMinimize() const {
  return !app_window_->show_on_lock_screen();
}

base::string16 NativeAppWindowViews::GetWindowTitle() const {
  return app_window_->GetTitle();
}

bool NativeAppWindowViews::ShouldShowWindowTitle() const {
  return false;
}

void NativeAppWindowViews::SaveWindowPlacement(const gfx::Rect& bounds,
                                               ui::WindowShowState show_state) {
  views::WidgetDelegate::SaveWindowPlacement(bounds, show_state);
  app_window_->OnNativeWindowChanged();
}

void NativeAppWindowViews::DeleteDelegate() {
  widget_->RemoveObserver(this);
  app_window_->OnNativeClose();
}

views::Widget* NativeAppWindowViews::GetWidget() {
  return widget_;
}

const views::Widget* NativeAppWindowViews::GetWidget() const {
  return widget_;
}

bool NativeAppWindowViews::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
#if defined(USE_AURA)
  if (child->Contains(web_view_->web_contents()->GetNativeView())) {
    // App window should claim mouse events that fall within the draggable
    // region.
    return !draggable_region_.get() ||
           !draggable_region_->contains(location.x(), location.y());
  }
#endif

  return true;
}

// WidgetObserver implementation.

void NativeAppWindowViews::OnWidgetDestroying(views::Widget* widget) {
  for (auto& observer : observer_list_)
    observer.OnHostDestroying();
}

void NativeAppWindowViews::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  app_window_->OnNativeWindowChanged();
}

void NativeAppWindowViews::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  app_window_->OnNativeWindowChanged();
  if (active)
    app_window_->OnNativeWindowActivated();
}

// WebContentsObserver implementation.

void NativeAppWindowViews::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (app_window_->requested_alpha_enabled() && CanHaveAlphaEnabled()) {
    content::RenderWidgetHostView* view =
        render_view_host->GetWidget()->GetView();
    DCHECK(view);
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
  } else if (app_window_->show_on_lock_screen()) {
    content::RenderWidgetHostView* view =
        render_view_host->GetWidget()->GetView();
    DCHECK(view);
    // When shown on the lock screen, app windows will be shown on top of black
    // background - to avoid a white flash while launching the app window,
    // initialize it with black background color.
    view->SetBackgroundColor(SK_ColorBLACK);
  }
}

void NativeAppWindowViews::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  OnViewWasResized();
}

// views::View implementation.

void NativeAppWindowViews::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    DCHECK(!web_view_);
    web_view_ = AddChildView(std::make_unique<views::WebView>(nullptr));
    web_view_->SetWebContents(app_window_->web_contents());
  }
}

gfx::Size NativeAppWindowViews::GetMinimumSize() const {
  return size_constraints_.GetMinimumSize();
}

gfx::Size NativeAppWindowViews::GetMaximumSize() const {
  return size_constraints_.GetMaximumSize();
}

void NativeAppWindowViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  OnViewWasResized();
}

void NativeAppWindowViews::OnFocus() {
  web_view_->RequestFocus();
}

// NativeAppWindow implementation.

void NativeAppWindowViews::SetFullscreen(int fullscreen_types) {
  // Stub implementation. See also ChromeNativeAppWindowViews.
  widget_->SetFullscreen(fullscreen_types !=
                         extensions::AppWindow::FULLSCREEN_TYPE_NONE);
}

bool NativeAppWindowViews::IsFullscreenOrPending() const {
  // Stub implementation. See also ChromeNativeAppWindowViews.
  return widget_->IsFullscreen();
}

void NativeAppWindowViews::UpdateWindowIcon() {
  widget_->UpdateWindowIcon();
}

void NativeAppWindowViews::UpdateWindowTitle() {
  widget_->UpdateWindowTitle();
}

void NativeAppWindowViews::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (!frameless_)
    return;

  draggable_region_.reset(
      extensions::AppWindow::RawDraggableRegionsToSkRegion(regions));
  OnViewWasResized();
}

SkRegion* NativeAppWindowViews::GetDraggableRegion() {
  return draggable_region_.get();
}

void NativeAppWindowViews::UpdateShape(std::unique_ptr<ShapeRects> rects) {
  // Stub implementation. See also ChromeNativeAppWindowViews.
}

bool NativeAppWindowViews::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

bool NativeAppWindowViews::IsFrameless() const {
  return frameless_;
}

bool NativeAppWindowViews::HasFrameColor() const {
  return false;
}

SkColor NativeAppWindowViews::ActiveFrameColor() const {
  return SK_ColorBLACK;
}

SkColor NativeAppWindowViews::InactiveFrameColor() const {
  return SK_ColorBLACK;
}

gfx::Insets NativeAppWindowViews::GetFrameInsets() const {
  if (frameless_)
    return gfx::Insets();

  // The pretend client_bounds passed in need to be large enough to ensure that
  // GetWindowBoundsForClientBounds() doesn't decide that it needs more than
  // the specified amount of space to fit the window controls in, and return a
  // number larger than the real frame insets. Most window controls are smaller
  // than 1000x1000px, so this should be big enough.
  gfx::Rect client_bounds = gfx::Rect(1000, 1000);
  gfx::Rect window_bounds =
      widget_->non_client_view()->GetWindowBoundsForClientBounds(client_bounds);
  return window_bounds.InsetsFrom(client_bounds);
}

gfx::Size NativeAppWindowViews::GetContentMinimumSize() const {
  return size_constraints_.GetMinimumSize();
}

gfx::Size NativeAppWindowViews::GetContentMaximumSize() const {
  return size_constraints_.GetMaximumSize();
}

void NativeAppWindowViews::SetContentSizeConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size) {
  size_constraints_.set_minimum_size(min_size);
  size_constraints_.set_maximum_size(max_size);
  widget_->OnSizeConstraintsChanged();
}

bool NativeAppWindowViews::CanHaveAlphaEnabled() const {
  return widget_->IsTranslucentWindowOpacitySupported();
}

void NativeAppWindowViews::SetVisibleOnAllWorkspaces(bool always_visible) {
  widget_->SetVisibleOnAllWorkspaces(always_visible);
}

void NativeAppWindowViews::SetActivateOnPointer(bool activate_on_pointer) {}

}  // namespace native_app_window
