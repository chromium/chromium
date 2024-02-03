// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_DEVICE_MONITORS_SYSTEM_MESSAGE_WINDOW_WIN_H_
#define MEDIA_DEVICE_MONITORS_SYSTEM_MESSAGE_WINDOW_WIN_H_

#include <windows.h>

#include <memory>

#include "media/base/media_export.h"

namespace gfx {
class SingletonHwndObserver;
}
namespace media {

class MEDIA_EXPORT SystemMessageWindowWin {
 public:
  SystemMessageWindowWin();

  SystemMessageWindowWin(const SystemMessageWindowWin&) = delete;
  SystemMessageWindowWin& operator=(const SystemMessageWindowWin&) = delete;

  virtual ~SystemMessageWindowWin();

  virtual LRESULT OnDeviceChange(UINT event_type, LPARAM data);

 private:
  void WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  class DeviceNotifications;
  std::unique_ptr<DeviceNotifications> device_notifications_;
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
};

}  // namespace media

#endif  // MEDIA_DEVICE_MONITORS_SYSTEM_MESSAGE_WINDOW_WIN_H_
