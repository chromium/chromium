// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_DESKTOP_MEDIA_WINDOW_REGISTRY_H_
#define CONTENT_BROWSER_MEDIA_DESKTOP_MEDIA_WINDOW_REGISTRY_H_

#include "ui/gfx/native_widget_types.h"

namespace content {

class DesktopMediaWindowRegistry {
 public:
  using Id = intptr_t;

  static DesktopMediaWindowRegistry* GetInstance();

  DesktopMediaWindowRegistry(const DesktopMediaWindowRegistry&) = delete;
  DesktopMediaWindowRegistry& operator=(const DesktopMediaWindowRegistry&) =
      delete;

  virtual ~DesktopMediaWindowRegistry();

  virtual Id RegisterWindow(gfx::NativeWindow window) = 0;
  virtual gfx::NativeWindow GetWindowById(Id id) = 0;

 protected:
  DesktopMediaWindowRegistry();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_DESKTOP_MEDIA_WINDOW_REGISTRY_H_
