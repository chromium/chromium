// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/no_destructor.h"
#include "content/browser/media/desktop_media_window_registry.h"

namespace content {

class DesktopMediaWindowRegistryMac : public DesktopMediaWindowRegistry {
 public:
  static DesktopMediaWindowRegistryMac* GetInstance() {
    static base::NoDestructor<DesktopMediaWindowRegistryMac> instance;
    return instance.get();
  }

  DesktopMediaWindowRegistryMac() = default;

  Id RegisterWindow(gfx::NativeWindow window) final {
    // Note that DesktopMediaPickerViews in //chrome depends on the fact that
    // the Id returned from this function is the NSWindow's windowNumber, but
    // that invariant is *not* part of the general contract for DesktopMediaID.
    return window.GetNativeNSWindow().windowNumber;
  }

  gfx::NativeWindow GetWindowById(Id id) final {
    return gfx::NativeWindow([NSApp windowWithWindowNumber:id]);
  }

 private:
  ~DesktopMediaWindowRegistryMac() final = default;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaWindowRegistryMac);
};

// static
DesktopMediaWindowRegistry* DesktopMediaWindowRegistry::GetInstance() {
  return DesktopMediaWindowRegistryMac::GetInstance();
}

}  // namespace content
