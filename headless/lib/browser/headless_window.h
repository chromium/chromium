// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "headless/lib/browser/headless_window_delegate.h"
#include "headless/public/headless_window_state.h"
#include "ui/gfx/geometry/rect.h"

namespace headless {

// Manages headless window visibility, bounds and state.
class HeadlessWindow {
 public:
  explicit HeadlessWindow(HeadlessWindowDelegate* delegate);

  HeadlessWindow(const HeadlessWindow&) = delete;
  HeadlessWindow& operator=(const HeadlessWindow&) = delete;

  ~HeadlessWindow();

  bool visible() const { return visible_; }
  gfx::Rect bounds() const { return bounds_; }
  HeadlessWindowState window_state() const { return window_state_; }

  void SetVisible(bool visible);
  void SetBounds(const gfx::Rect& bounds);
  void SetWindowState(HeadlessWindowState window_state);

 protected:
  void ZoomWindowBounds();
  void RestoreWindowBounds();

  void UpdateVisible(bool visible);
  void UpdateBounds(const gfx::Rect& bounds);
  void UpdateWindowState(HeadlessWindowState window_state);

 private:
  raw_ptr<HeadlessWindowDelegate> delegate_;

  bool visible_ = false;
  gfx::Rect bounds_;
  std::optional<gfx::Rect> restored_bounds_;
  HeadlessWindowState window_state_ = HeadlessWindowState::kNormal;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_H_
