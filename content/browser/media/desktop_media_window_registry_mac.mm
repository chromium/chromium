// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/no_destructor.h"
#include "content/browser/media/desktop_media_window_registry.h"

namespace content {

class DesktopMediaWindowRegistryMac final : public DesktopMediaWindowRegistry {
 public:
  static DesktopMediaWindowRegistryMac* GetInstance() {
    static base::NoDestructor<DesktopMediaWindowRegistryMac> instance;
    return instance.get();
  }

  DesktopMediaWindowRegistryMac() = default;

  DesktopMediaWindowRegistryMac(const DesktopMediaWindowRegistryMac&) = delete;
  DesktopMediaWindowRegistryMac& operator=(
      const DesktopMediaWindowRegistryMac&) = delete;

  // Note that DesktopMediaPickerViews in //chrome depends on the fact that
  // the Id returned from this function is the NSWindow's windowNumber, but
  // that invariant is *not* part of the general contract for DesktopMediaID.
  DesktopMediaID::Id RegisterWindow(gfx::NativeWindow window) override {
    // Ensure that we don't inadvertently crop IDs.
    static_assert(sizeof(NSInteger) == sizeof(DesktopMediaID::Id));
    return window.GetNativeNSWindow().windowNumber;
  }

  gfx::NativeWindow GetWindowById(DesktopMediaID::Id id) override {
    return gfx::NativeWindow([NSApp windowWithWindowNumber:id]);
  }

 private:
  ~DesktopMediaWindowRegistryMac() override = default;
};

// static
DesktopMediaWindowRegistry* DesktopMediaWindowRegistry::GetInstance() {
  return DesktopMediaWindowRegistryMac::GetInstance();
}

}  // namespace content
