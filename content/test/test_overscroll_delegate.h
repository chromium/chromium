// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_OVERSCROLL_DELEGATE_H_
#define CONTENT_TEST_TEST_OVERSCROLL_DELEGATE_H_

#include <optional>
#include <vector>

#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class TestOverscrollDelegate : public OverscrollControllerDelegate {
 public:
  explicit TestOverscrollDelegate(const gfx::Size& display_size);

  TestOverscrollDelegate(const TestOverscrollDelegate&) = delete;
  TestOverscrollDelegate& operator=(const TestOverscrollDelegate&) = delete;

  ~TestOverscrollDelegate() override;

  void set_delta_cap(float delta_cap) { delta_cap_ = delta_cap; }

  OverscrollMode current_mode() const { return current_mode_; }
  OverscrollMode completed_mode() const { return completed_mode_; }
  const std::vector<OverscrollMode>& historical_modes() const {
    return historical_modes_;
  }
  float delta_x() const { return delta_x_; }
  float delta_y() const { return delta_y_; }

  void Reset();

 private:
  // Overridden from OverscrollControllerDelegate:
  gfx::Size GetDisplaySize() const override;
  bool OnOverscrollUpdate(float delta_x, float delta_y) override;
  void OnOverscrollComplete(OverscrollMode overscroll_mode) override;
  void OnOverscrollModeChange(OverscrollMode old_mode,
                              OverscrollMode new_mode,
                              OverscrollSource source,
                              cc::OverscrollBehavior behavior) override;
  std::optional<float> GetMaxOverscrollDelta() const override;

  gfx::Size display_size_;

  std::optional<float> delta_cap_;
  OverscrollMode current_mode_;
  OverscrollMode completed_mode_;
  std::vector<OverscrollMode> historical_modes_;

  float delta_x_;
  float delta_y_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_OVERSCROLL_DELEGATE_H_
