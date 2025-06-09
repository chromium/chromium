// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_DELEGATE_H_

#include "headless/public/headless_window_state.h"
#include "ui/gfx/geometry/rect.h"

namespace headless {

class HeadlessWindowDelegate {
 public:
  HeadlessWindowDelegate() = default;

  HeadlessWindowDelegate(const HeadlessWindowDelegate&) = delete;
  HeadlessWindowDelegate& operator=(const HeadlessWindowDelegate&) = delete;

  virtual ~HeadlessWindowDelegate() = default;

  virtual void OnVisibilityChanged() {}
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds) {}
  virtual void OnWindowStateChanged(HeadlessWindowState old_window_state) {}
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_DELEGATE_H_
