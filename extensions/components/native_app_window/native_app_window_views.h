// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMPONENTS_NATIVE_APP_WINDOW_NATIVE_APP_WINDOW_VIEWS_H_
#define EXTENSIONS_COMPONENTS_NATIVE_APP_WINDOW_NATIVE_APP_WINDOW_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/size_constraints.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class SkRegion;

namespace views {
class WebView;
}  // namespace views

namespace gfx {
class RoundedCornersF;
class Rect;
}  // namespace gfx

namespace native_app_window {

// A NativeAppWindow backed by a views::Widget. This class may be used alone
// as a stub or subclassed (for example, ChromeNativeAppWindowViews).
class NativeAppWindowViews : public extensions::NativeAppWindow,
                             public content::WebContentsObserver,
                             public views::WidgetDelegateView,
                             public views::WidgetObserver {
  METADATA_HEADER(NativeAppWindowViews, views::WidgetDelegateView)

 public:
  NativeAppWindowViews();
  NativeAppWindowViews(const NativeAppWindowViews&) = delete;
  NativeAppWindowViews& operator=(const NativeAppWindowViews&) = delete;
  ~NativeAppWindowViews() override;
  void Init(extensions::AppWindow* app_window,
            const extensions::AppWindow::CreateParams& create_params);

  // Signal that CanHaveTransparentBackground has changed.
  void OnCanHaveAlphaEnabledChanged();

  extensions::AppWindow* app_window() { return app_window_; }
  const extensions::AppWindow* app_window() const { return app_window_; }

  views::WebView* web_view() { return web_view_; }
  const views::WebView* web_view() const { return web_view_; }

  views::Widget* widget() { return widget_; }
  const views::Widget* widget() const { return widget_; }

  void set_window_for_testing(views::Widget* window) { widget_ = window; }
  void set_web_view_for_testing(views::WebView* view) { web_view_ = view; }

 protected:
  // Initializes |widget_| for |app_window|.
  virtual void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params);

  // ui::BaseWindow:
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() const override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void FlashFrame(bool flash) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;

  // WidgetDelegate:
  void OnWidgetMove() override;
  views::View* GetInitiallyFocusedView() override;
  std::u16string GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldSaveWindowPlacement() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::mojom::WindowShowState show_state) override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

  // views::View:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnFocus() override;

  // NativeAppWindow:
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
  gfx::Size GetContentMinimumSize() const override;
  gfx::Size GetContentMaximumSize() const override;
  void SetContentSizeConstraints(const gfx::Size& min_size,
                                 const gfx::Size& max_size) override;
  bool CanHaveAlphaEnabled() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  void SetActivateOnPointer(bool activate_on_pointer) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  void OnWidgetHasHitTestMaskChanged();

 private:
  // Informs modal dialogs that they need to update their positions.
  void OnViewWasResized();

  bool GetCanMaximizeWindow() const;
  bool GetCanResizeWindow() const;

  raw_ptr<extensions::AppWindow> app_window_ = nullptr;  // Not owned.
  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<views::Widget, DanglingUntriaged> widget_ = nullptr;

  std::unique_ptr<SkRegion> draggable_region_;

  bool frameless_ = false;
  bool resizable_ = false;
  extensions::SizeConstraints size_constraints_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      observer_list_;
};

}  // namespace native_app_window

#endif  // EXTENSIONS_COMPONENTS_NATIVE_APP_WINDOW_NATIVE_APP_WINDOW_VIEWS_H_
