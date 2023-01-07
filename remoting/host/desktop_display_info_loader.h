// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_LOADER_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_LOADER_H_

#include <memory>

#include "remoting/host/desktop_display_info.h"

namespace remoting {

// Interface for retrieving the current display info from the OS. All methods
// should be called on the UI thread, though the instance may be constructed on
// another thread.
class DesktopDisplayInfoLoader {
 public:
  // Can be called on any thread.
  static std::unique_ptr<DesktopDisplayInfoLoader> Create();

  DesktopDisplayInfoLoader(const DesktopDisplayInfoLoader&) = delete;
  DesktopDisplayInfoLoader& operator=(const DesktopDisplayInfoLoader&) = delete;

  // Object should be destroyed on the UI thread.
  virtual ~DesktopDisplayInfoLoader() = default;

  // Optional initialization performed on the UI thread.
  virtual void Init() {}

  // Returns the current display info. Implementations may retrieve this
  // directly from the OS each time, or may return information from an
  // internal cache and update this cache in response to an OS-level
  // "displays-changed" notification.
  virtual DesktopDisplayInfo GetCurrentDisplayInfo() = 0;

 protected:
  DesktopDisplayInfoLoader() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_LOADER_H_
