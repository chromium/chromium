// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_TREE_HOST_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_TREE_HOST_H_

#if defined(USE_AURA)

#include <memory>

#include "ui/aura/window_tree_host.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
namespace client {
class FocusClient;
class WindowParentingClient;
}
}

namespace ui {
enum class DomCode : uint32_t;
}

namespace headless {

class HeadlessWindowTreeHost : public aura::WindowTreeHost,
                               public ui::PlatformEventDispatcher {
 public:
  explicit HeadlessWindowTreeHost(bool use_external_begin_frame_control);

  HeadlessWindowTreeHost(const HeadlessWindowTreeHost&) = delete;
  HeadlessWindowTreeHost& operator=(const HeadlessWindowTreeHost&) = delete;

  ~HeadlessWindowTreeHost() override;

  void SetParentWindow(gfx::NativeWindow window);

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnScreenInPixels() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool CaptureSystemKeyEventsImpl(
      std::optional<base::flat_set<ui::DomCode>> codes) override;
  void ReleaseSystemKeyEventCapture() override;
  bool IsKeyLocked(ui::DomCode dom_code) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  void SetCursorNative(gfx::NativeCursor cursor_type) override;
  void MoveCursorToScreenLocationInPixels(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::string GetUniqueId() const override;
#endif

 private:
  gfx::Rect bounds_;
  std::unique_ptr<aura::client::FocusClient> focus_client_;
  std::unique_ptr<aura::client::WindowParentingClient> window_parenting_client_;
};

}  // namespace headless

#else   // defined(USE_AURA)
class HeadlessWindowTreeHost {};
#endif  // defined(USE_AURA)

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_TREE_HOST_H_
