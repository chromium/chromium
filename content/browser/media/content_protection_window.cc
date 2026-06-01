// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/content_protection_window.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/win/wrapped_window_proc.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace content {

namespace {

// Process-wide window class atom for `ContentProtectionWindow`'s HWNDs.
ATOM g_content_protection_window_class = 0;

// Lazily registers the window class. Returns `true`
// on success or if the class is already registered.
bool EnsureWindowClassRegistered() {
  if (g_content_protection_window_class) {
    return true;
  }

  WNDCLASSEX wc;
  base::win::InitializeWindowClass(
      L"MediaFoundationContentProtectionWindow",
      &base::win::WrappedWindowProc<::DefWindowProc>, /*style=*/0,
      /*class_extra=*/0, /*window_extra=*/0, /*cursor=*/nullptr,
      /*background=*/nullptr, /*menu_name=*/nullptr, /*icon=*/nullptr,
      /*small_icon=*/nullptr, &wc);
  g_content_protection_window_class = ::RegisterClassEx(&wc);
  if (!g_content_protection_window_class) {
    PLOG(ERROR) << "RegisterClassEx failed for "
                   "MediaFoundationContentProtectionWindow";
    return false;
  }
  return true;
}

// Returns the top-level browser HWND for `aura_window`, or `nullptr` if
// `aura_window` is detached from a host (e.g. mid-reparent or tearing
// down) or no native widget is available.
HWND GetTopLevelHwndForWindow(aura::Window* aura_window) {
  if (!aura_window) {
    return nullptr;
  }
  aura::WindowTreeHost* host = aura_window->GetHost();
  if (!host) {
    return nullptr;
  }
  return host->GetAcceleratedWidget();
}

}  // namespace

// static
ContentProtectionWindowOrStatus ContentProtectionWindow::Create(
    RenderFrameHost* render_frame_host) {
  if (!EnsureWindowClassRegistered()) {
    return base::unexpected(
        ContentProtectionWindowStatus::kClassRegistrationFailed);
  }

  // Use the outermost main frame's view because that's where the renderer's
  // native window lives; cross-origin iframes don't have their own
  // top-level HWND.
  RenderWidgetHostView* view =
      render_frame_host->GetOutermostMainFrame()->GetView();
  if (!view) {
    return base::unexpected(ContentProtectionWindowStatus::kNoView);
  }

  gfx::NativeView native_view = view->GetNativeView();
  if (!native_view) {
    return base::unexpected(ContentProtectionWindowStatus::kNoNativeView);
  }

  HWND parent = GetTopLevelHwndForWindow(native_view);
  if (!parent) {
    return base::unexpected(ContentProtectionWindowStatus::kNoTopLevelHwnd);
  }

  // The HWND is a hidden child sized to cover the parent's full client area:
  //  - `WS_CHILD` so it follows the parent across monitor moves and tracks
  //    the parent's monitor via `MonitorFromWindow()`.
  //  - Sized to the parent's client rect so that `MonitorFromWindow()`
  //    computes the same monitor overlap as the parent. This matters when
  //    the browser window straddles two monitors driven by different GPUs.
  //  - `WS_DISABLED` to never receive input.
  //  - `WS_EX_NOPARENTNOTIFY | WS_EX_TRANSPARENT | WS_EX_LAYERED |
  //    WS_EX_NOREDIRECTIONBITMAP` to avoid input notifications and avoid
  //    allocating a redirection bitmap (the window is never drawn).
  // It is intentionally not `WS_VISIBLE`: Media Foundation only reads the
  // HWND's monitor, so it does not need to be visible.
  RECT client_rect = {};
  ::GetClientRect(parent, &client_rect);
  HWND hwnd = ::CreateWindowEx(
      WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_TRANSPARENT |
          WS_EX_NOREDIRECTIONBITMAP,
      reinterpret_cast<wchar_t*>(g_content_protection_window_class), L"",
      WS_CHILD | WS_DISABLED, /*x=*/0, /*y=*/0,
      /*nWidth=*/client_rect.right, /*nHeight=*/client_rect.bottom, parent,
      /*hMenu=*/nullptr, /*hInstance=*/nullptr, /*lpParam=*/nullptr);
  if (!hwnd) {
    PLOG(ERROR) << "CreateWindowEx failed for content protection window";
    return base::unexpected(
        ContentProtectionWindowStatus::kCreateWindowExFailed);
  }

  return base::WrapUnique(new ContentProtectionWindow(hwnd, native_view));
}

ContentProtectionWindow::ContentProtectionWindow(
    HWND hwnd,
    aura::Window* observed_native_view)
    : observed_native_view_(observed_native_view), hwnd_(hwnd) {
  observed_native_view_->AddObserver(this);
}

ContentProtectionWindow::~ContentProtectionWindow() {
  Cleanup();
}

void ContentProtectionWindow::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, observed_native_view_);
  // Fires when the observed `aura::Window` is attached to a root, including
  // after a tab is dragged into another browser window or popped into its
  // own window.
  ReparentToCurrentTopLevel();
}

void ContentProtectionWindow::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window, observed_native_view_);
  ResizeToMatchParent();
}

void ContentProtectionWindow::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, observed_native_view_);
  // Destroy the HWND now while it is still valid. The top-level browser
  // HWND (our parent) is destroyed after aura child windows, so our child
  // HWND is still alive at this point. If we deferred to the destructor,
  // the parent cascade could destroy it first, leaving a stale handle.
  Cleanup();
}

void ContentProtectionWindow::Cleanup() {
  if (observed_native_view_) {
    observed_native_view_->RemoveObserver(this);
    observed_native_view_ = nullptr;
  }
  if (hwnd_) {
    ::DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

void ContentProtectionWindow::ReparentToCurrentTopLevel() {
  if (!hwnd_) {
    return;
  }
  HWND new_parent = GetTopLevelHwndForWindow(observed_native_view_);
  if (!new_parent) {
    // Mid-reparent: no root yet. Don't touch the parent now;
    // `OnWindowAddedToRootWindow` will fire again with the new root.
    return;
  }
  if (::GetParent(hwnd_) == new_parent) {
    return;
  }
  if (!::SetParent(hwnd_, new_parent)) {
    PLOG(ERROR) << "SetParent failed for content protection window";
    return;
  }

  ResizeToMatchParent();
}

void ContentProtectionWindow::ResizeToMatchParent() {
  if (!hwnd_) {
    return;
  }
  HWND parent = ::GetParent(hwnd_);
  if (!parent) {
    return;
  }
  RECT client_rect = {};
  if (::GetClientRect(parent, &client_rect)) {
    ::SetWindowPos(hwnd_, nullptr, 0, 0, client_rect.right, client_rect.bottom,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

}  // namespace content
