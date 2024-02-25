// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_LOADER_X11_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_LOADER_X11_H_

#include "base/memory/raw_ptr.h"
#include "remoting/host/desktop_display_info_loader.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/window_event_manager.h"

namespace remoting {

class DesktopDisplayInfoLoaderX11 : public DesktopDisplayInfoLoader,
                                    public x11::EventObserver {
 public:
  DesktopDisplayInfoLoaderX11();
  ~DesktopDisplayInfoLoaderX11() override;

  // DesktopDisplayInfoLoader implementation.
  void Init() override;
  DesktopDisplayInfo GetCurrentDisplayInfo() override;

  // x11::EventObserver implementation.
  void OnEvent(const x11::Event& xevent) override;

 private:
  // Queries the X server and updates |monitors_|.
  void LoadMonitors();

  raw_ptr<x11::Connection> connection_ = nullptr;
  raw_ptr<x11::RandR> randr_ = nullptr;

  // Selector for root window events.
  x11::ScopedEventSelector root_window_events_;

  std::vector<x11::RandR::MonitorInfo> monitors_;
};

}  // namespace remoting
#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_LOADER_X11_H_
