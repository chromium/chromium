// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_DISPLAY_H_
#define REMOTING_HOST_LINUX_WAYLAND_DISPLAY_H_

#include <string>
#include <vector>

#include <xdg-output-unstable-v1-client-protocol.h>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/linux/wayland_display_info.h"

namespace remoting {

// This class gathers information about the Wayland Output / Display by
// binding to wl_output and zxdg_output_v1 interfaces provided by the protocol.
class WaylandDisplay {
 public:
  WaylandDisplay();
  ~WaylandDisplay();

  WaylandDisplay(const WaylandDisplay&) = delete;
  WaylandDisplay& operator=(const WaylandDisplay&) = delete;

  // Handles output related global events (that are emitted for each display
  // present on the system).
  // Does not take ownership of |registry|
  void HandleGlobalDisplayEvent(struct wl_registry* registry,
                                uint32_t name,
                                const char* interface,
                                uint32_t version);

  // Returns true if the global object being removed was in fact a display
  // object.
  bool HandleGlobalRemoveDisplayEvent(uint32_t name);

  DesktopDisplayInfo GetCurrentDisplayInfo() const;

 private:
  void InitXdgOutputIfPossible(DisplayInfo& display_info);
  void FinishPartialXdgOutputInitializations();
  static void UpdateDisplayInfo(DisplayInfo display_info,
                                DisplayInfo& existing_info);

  // wl_output interface event handlers.
  static void OnGeometryEvent(void* data,
                              wl_output* wl_output,
                              int x,
                              int y,
                              int physical_width,
                              int physical_height,
                              int subpixel,
                              const char* make,
                              const char* model,
                              int transform);
  static void OnModeEvent(void* data,
                          wl_output* wl_output,
                          uint flags,
                          int width,
                          int height,
                          int refresh);
  static void OnDoneEvent(void* data, wl_output* wl_output);
  static void OnScaleEvent(void* data, wl_output* wl_output, int factor);

  // zxdg_output_v1 interface event handlers.
  static void OnXdgOutputLogicalPositionEvent(void* data,
                                              struct zxdg_output_v1* xdg_output,
                                              int32_t x,
                                              int32_t y);
  static void OnXdgOutputLogicalSizeEvent(void* data,
                                          struct zxdg_output_v1* xdg_output,
                                          int32_t width,
                                          int32_t height);
  static void OnXdgOutputDoneEvent(void* data,
                                   struct zxdg_output_v1* xdg_output);
  static void OnXdgOutputNameEvent(void* data,
                                   struct zxdg_output_v1* xdg_output,
                                   const char* name);
  static void OnXdgOutputDescriptionEvent(void* data,
                                          struct zxdg_output_v1* xdg_output,
                                          const char* description);

  SEQUENCE_CHECKER(sequence_checker_);

  const struct wl_output_listener wl_output_listener_;
  const struct zxdg_output_v1_listener xdg_output_listener_;
  raw_ptr<struct zxdg_output_manager_v1> xdg_output_manager_ = nullptr;
  std::vector<DisplayInfo> display_info_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_DISPLAY_H_
