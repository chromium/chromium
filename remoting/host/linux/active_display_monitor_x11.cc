// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/active_display_monitor_x11.h"

#include <memory>

#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/window_event_manager.h"

namespace remoting {

namespace {

// For X11, webrtc::ScreenId is implemented as a RANDR Monitor ID, which
// requires XRANDR 1.5.
constexpr std::pair<uint32_t, uint32_t> kMinRandrVersion{1, 5};

}  // namespace

// This class uses Chromium X11 utilities which create global singletons, so
// this code needs to run on the UI thread.
class ActiveDisplayMonitorX11::Core : public x11::EventObserver {
 public:
  // `callback` will be run on the UI thread whenever the active display is
  // changed. The callback is responsible for posting a task back to the
  // caller's thread.
  explicit Core(ActiveDisplayMonitor::Callback callback);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  void Init();

  // x11::EventObserver implementation.
  void OnEvent(const x11::Event& xevent) override;

 private:
  // Called when the active display might have changed. This gets the
  // currently-focused window and determines which display it is on. If the
  // display is changed, it is sent to `callback_`.
  void GetAndSendActiveDisplay();

  // Returns the window with input-focus, or x11::Window::None. The returned
  // window may be a descendent of a top-level window.
  x11::Window GetFocusedWindow() const;

  // Returns the ancestor of 'window' which is a child of the root window. The
  // returned window may be a window-manager frame. On error, this returns
  // x11::Window::None.
  x11::Window GetTopLevelWindow(x11::Window window) const;

  bool IsWindowVisible(x11::Window window) const;

  // Returns the display that the window is positioned on. On error, this
  // returns webrtc::kInvalidScreenId.
  webrtc::ScreenId GetDisplayForWindow(x11::Window window) const;

  ActiveDisplayMonitor::Callback callback_;

  raw_ptr<x11::Connection> connection_ = nullptr;

  x11::ScopedEventSelector root_window_event_selector_;

  x11::Atom net_active_window_atom_{};

  webrtc::ScreenId current_active_display_{webrtc::kInvalidScreenId};
};

////////////////////////////////////////////////////////////////////////////////
// ActiveDisplayMonitorX11::Core implementation.

ActiveDisplayMonitorX11::Core::Core(ActiveDisplayMonitor::Callback callback)
    : callback_(callback) {}

ActiveDisplayMonitorX11::Core::~Core() {
  // Does nothing if AddEventObserver() was not called.
  connection_->RemoveEventObserver(this);
}

void ActiveDisplayMonitorX11::Core::Init() {
  connection_ = x11::Connection::Get();
  auto xrandr_version = connection_->randr_version();
  if (xrandr_version < kMinRandrVersion) {
    LOG(ERROR) << "XRANDR version (" << xrandr_version.first << ", "
               << xrandr_version.second << ") is unsupported.";
    return;
  }

  root_window_event_selector_ = connection_->ScopedSelectEvent(
      ui::GetX11RootWindow(), x11::EventMask::PropertyChange);

  net_active_window_atom_ = x11::GetAtom("_NET_ACTIVE_WINDOW");

  connection_->AddEventObserver(this);
}

void ActiveDisplayMonitorX11::Core::OnEvent(const x11::Event& xevent) {
  const auto* property_notify = xevent.As<x11::PropertyNotifyEvent>();
  if (!property_notify) {
    return;
  }

  if (property_notify->window != ui::GetX11RootWindow()) {
    return;
  }

  if (property_notify->atom != net_active_window_atom_) {
    return;
  }

  // Check the property was not deleted.
  if (property_notify->state != x11::Property::NewValue) {
    return;
  }

  GetAndSendActiveDisplay();
}

void ActiveDisplayMonitorX11::Core::GetAndSendActiveDisplay() {
  x11::Window focused_window = GetFocusedWindow();
  if (focused_window == x11::Window::None) {
    HOST_LOG << "No window is focused.";
    return;
  }

  focused_window = GetTopLevelWindow(focused_window);
  if (focused_window == x11::Window::None) {
    return;
  }

  if (!IsWindowVisible(focused_window)) {
    // It's not clear if a window-manager would ever move input-focus to a
    // hidden window? It seems safer to not activate a different display if the
    // user can't see the window on it.
    HOST_LOG << "Focused window " << base::to_underlying(focused_window)
             << " is hidden, ignoring.";
    return;
  }

  webrtc::ScreenId active_display = GetDisplayForWindow(focused_window);
  if (active_display == webrtc::kInvalidScreenId) {
    LOG(ERROR) << "Failed to determine display for window "
               << base::to_underlying(focused_window);
    return;
  }

  if (active_display == current_active_display_) {
    return;
  }

  current_active_display_ = active_display;
  HOST_LOG << "Active display changed to " << active_display
           << " due to window " << base::to_underlying(focused_window);
  callback_.Run(active_display);
}

x11::Window ActiveDisplayMonitorX11::Core::GetFocusedWindow() const {
  x11::Window focused_window;
  if (!x11::Connection::Get()->GetPropertyAs(
          ui::GetX11RootWindow(), net_active_window_atom_, &focused_window)) {
    LOG(ERROR) << "Failed to read _NET_ACTIVE_WINDOW on root window.";
    return x11::Window::None;
  }

  return focused_window;
}

x11::Window ActiveDisplayMonitorX11::Core::GetTopLevelWindow(
    x11::Window window) const {
  // This will not loop infinitely, as long as the X11 server respects the
  // protocol and provides a correct tree of windows.
  while (true) {
    auto query_response = connection_->QueryTree({window}).Sync();
    if (!query_response) {
      LOG(ERROR) << "QueryTree failed for window "
                 << base::to_underlying(window);
      return x11::Window::None;
    }
    if (query_response->parent == x11::Window::None) {
      // The root window was provided, so just return it directly. This is not
      // expected to happen, but any program could set arbitrary values for the
      // root window's _NET_ACTIVE_WINDOW property, so this code should handle
      // all possibilities.
      break;
    }
    if (query_response->root == query_response->parent) {
      // Found top-level window.
      break;
    }

    window = query_response->parent;
  }
  return window;
}

bool ActiveDisplayMonitorX11::Core::IsWindowVisible(x11::Window window) const {
  auto attributes = connection_->GetWindowAttributes({window}).Sync();
  if (!attributes) {
    LOG(ERROR) << "Failed to get attributes for window "
               << base::to_underlying(window);
    return false;
  }
  return attributes->map_state == x11::MapState::Viewable;
}

webrtc::ScreenId ActiveDisplayMonitorX11::Core::GetDisplayForWindow(
    x11::Window window) const {
  webrtc::ScreenId result = webrtc::kInvalidScreenId;

  auto geometry = connection_->GetGeometry(window).Sync();
  if (!geometry) {
    LOG(ERROR) << "GetGeometry() failed for window "
               << base::to_underlying(window);
    return result;
  }

  auto monitors =
      connection_->randr().GetMonitors({ui::GetX11RootWindow()}).Sync();
  if (!monitors) {
    LOG(ERROR) << "RANDR GetMonitors() failed.";
    return result;
  }

  auto window_rect = webrtc::DesktopRect::MakeXYWH(
      geometry->x, geometry->y, geometry->width, geometry->height);
  int best_area = 0;
  for (const x11::RandR::MonitorInfo& info : monitors->monitors) {
    auto monitor_rect =
        webrtc::DesktopRect::MakeXYWH(info.x, info.y, info.width, info.height);
    monitor_rect.IntersectWith(window_rect);
    int area = monitor_rect.width() * monitor_rect.height();

    // Strict inequality ensures that kInvalidScreenId is returned in the case
    // that all overlaps have zero area.
    if (area > best_area) {
      // The X11 desktop-capturer uses the `name` atom as the screen ID.
      result = base::to_underlying(info.name);
      best_area = area;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
// ActiveDisplayMonitorX11 implementation.

ActiveDisplayMonitorX11::ActiveDisplayMonitorX11(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    Callback active_display_callback)
    : active_display_callback_(active_display_callback) {
  Callback wrapped_callback =
      base::BindPostTaskToCurrentDefault(active_display_callback_.callback());
  core_.emplace(ui_task_runner, wrapped_callback);
  core_.AsyncCall(&ActiveDisplayMonitorX11::Core::Init);
}

ActiveDisplayMonitorX11::~ActiveDisplayMonitorX11() = default;

}  // namespace remoting
