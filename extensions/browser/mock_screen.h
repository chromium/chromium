// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOCK_SCREEN_H_
#define EXTENSIONS_BROWSER_MOCK_SCREEN_H_

#include <vector>

#include "ui/display/screen.h"

namespace extensions {

class MockScreen : public display::Screen {
 public:
  MockScreen();
  ~MockScreen() override;
  MockScreen(const MockScreen&) = delete;
  MockScreen& operator=(const MockScreen&) = delete;

 protected:
  // Overridden from display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override {}
  void RemoveObserver(display::DisplayObserver* observer) override {}

 private:
  std::vector<display::Display> displays_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOCK_SCREEN_H_
