// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CONTENT_PROTECTION_WINDOW_H_
#define CONTENT_BROWSER_MEDIA_CONTENT_PROTECTION_WINDOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"
#include "content/common/content_export.h"
#include "ui/aura/window_observer.h"

namespace content {

class ContentProtectionWindow;
class RenderFrameHost;

// Outcome of `ContentProtectionWindow::Create()`. Recorded to UMA, so values
// must not be renumbered. Keep in sync with `ContentProtectionWindowStatus`
// in tools/metrics/histograms/metadata/media/enums.xml.
enum class ContentProtectionWindowStatus {
  // Window creation succeeded. Only used for UMA.
  kSuccess = 0,
  // `RegisterClassEx` failed. Effectively permanent for the process.
  kClassRegistrationFailed = 1,
  // The outermost main frame has no `RenderWidgetHostView`
  kNoView = 2,
  // The `RenderWidgetHostView` has no `gfx::NativeView`.
  kNoNativeView = 3,
  // The aura window has no top-level browser HWND (e.g. detached
  // mid-reparent, or headless mode).
  kNoTopLevelHwnd = 4,
  // `CreateWindowEx` failed.
  kCreateWindowExFailed = 5,
  kMaxValue = kCreateWindowExFailed,
};

using ContentProtectionWindowOrStatus =
    base::expected<std::unique_ptr<ContentProtectionWindow>,
                   ContentProtectionWindowStatus>;

// Browser-side helper that owns a hidden child HWND parented to the top-level
// browser window of the frame's outermost main frame. The HWND is intended to
// be passed to the Media Foundation CDM utility process via
// `FrameInterfaceFactory::GetContentProtectionWindow()` and wrapped in an
// `ICoreWindow` which is then passed to Media Foundation using
// `IsTypeSupportedEx`. Media Foundation uses this window to pick the correct
// GPU adapter which is driving the monitor where the browser window currently
// lives.
//
// Topology change handling:
//
// - User drags the browser window across monitors: handled implicitly. The
//   HWND is a child of the browser top-level, so it follows
//   automatically.
//
// - Tab dragged into another browser window (or popped into its own
//   window): observed via `WindowObserver::OnWindowAddedToRootWindow`.
//   The HWND is reparented to the new top-level browser HWND.
//
// - Browser window/tab destroyed: observed via `OnWindowDestroying`. The
//   HWND is destroyed and the observer is cleared.
//
// - Browser window resized: observed via
//   `WindowObserver::OnWindowBoundsChanged`. The HWND is resized to match
//   the parent's client area so that adapter determination correctly handles
//   cases where the browser window straddles two monitors.
//
// PiP playback on a monitor different from the tab's browser window is not
// covered by this design (the `<video>` is still owned by the original
// frame's RenderFrameHost, so the tab's top-level HWND is the one we track).
//
// Lifetime: created lazily by `FrameInterfaceFactoryImpl` on the first
// `GetContentProtectionWindow()` call. Destroyed when the owning
// `FrameInterfaceFactoryImpl` is torn down, which happens on either
// document navigation or utility-process disconnect.
// Note: `render_frame_host` must outlive `this` class.
class CONTENT_EXPORT ContentProtectionWindow : public aura::WindowObserver {
 public:
  // Attempts to create a `ContentProtectionWindow` for `render_frame_host`.
  // Returns the new instance on success, or a `ContentProtectionWindowStatus`
  // describing why creation failed.
  static ContentProtectionWindowOrStatus Create(
      RenderFrameHost* render_frame_host);

  ContentProtectionWindow(const ContentProtectionWindow&) = delete;
  ContentProtectionWindow& operator=(const ContentProtectionWindow&) = delete;

  ~ContentProtectionWindow() override;

  // Returns the HWND. Non-null on a live instance; may become `nullptr` if
  // the observed aura window is destroyed before `this`.
  HWND hwnd() const { return hwnd_; }

 private:
  ContentProtectionWindow(HWND hwnd, aura::Window* observed_native_view);

  // aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Removes the observer and destroys the HWND.
  void Cleanup();

  // Reparents `hwnd_` to the current top-level HWND
  // if it differs from the existing parent.
  void ReparentToCurrentTopLevel();

  // Resizes `hwnd_` to cover its parent's full client area so that the
  // HWND has the same monitor overlap as the browser window.
  void ResizeToMatchParent();

  // The outermost main frame's `aura::Window` (the RWHV's native view).
  raw_ptr<aura::Window> observed_native_view_ = nullptr;

  // The browser-owned, hidden HWND.
  HWND hwnd_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CONTENT_PROTECTION_WINDOW_H_
