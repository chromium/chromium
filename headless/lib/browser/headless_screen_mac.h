// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_MAC_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_MAC_H_

#include <memory>
#include <string_view>

#include "headless/lib/browser/headless_screen.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

class HeadlessScreenMac : public HeadlessScreen {
 public:
  static HeadlessScreenMac* Create(const gfx::Size& window_size,
                                   std::string_view screen_info_spec);

  HeadlessScreenMac(const HeadlessScreenMac&) = delete;
  HeadlessScreenMac& operator=(const HeadlessScreenMac&) = delete;

  ~HeadlessScreenMac() override;

  // display::Screen overrides:
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override;

 private:
  class ClassSwizzler;

  HeadlessScreenMac(const gfx::Size& window_size,
                    std::string_view screen_info_spec);

  std::unique_ptr<ClassSwizzler> class_swizzler_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_MAC_H_
