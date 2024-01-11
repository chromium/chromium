// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/common/content_export.h"

namespace content {

class Affordance;
class WebContentsImpl;

// A simple delegate for the overscroll controller that paints an arrow on top
// of the web-contents as a hint for pending navigations from overscroll.
class CONTENT_EXPORT GestureNavSimple : public OverscrollControllerDelegate {
 public:
  explicit GestureNavSimple(WebContentsImpl* web_contents);

  GestureNavSimple(const GestureNavSimple&) = delete;
  GestureNavSimple& operator=(const GestureNavSimple&) = delete;

  ~GestureNavSimple() override;

  // Called by the affordance when its complete/abort animation is finished so
  // that the affordance instance can be destroyed.
  void OnAffordanceAnimationEnded();

 private:
  friend class GestureNavSimpleTest;

  // OverscrollControllerDelegate:
  gfx::Size GetDisplaySize() const override;
  bool OnOverscrollUpdate(float delta_x, float delta_y) override;
  void OnOverscrollComplete(OverscrollMode overscroll_mode) override;
  void OnOverscrollModeChange(OverscrollMode old_mode,
                              OverscrollMode new_mode,
                              OverscrollSource source,
                              cc::OverscrollBehavior behavior) override;
  std::optional<float> GetMaxOverscrollDelta() const override;

  raw_ptr<WebContentsImpl> web_contents_ = nullptr;

  OverscrollMode mode_ = OVERSCROLL_NONE;
  OverscrollSource source_ = OverscrollSource::NONE;
  std::unique_ptr<Affordance> affordance_;
  float completion_threshold_ = 0.f;

  // When an overscroll is active, represents the maximum overscroll delta we
  // expect in OnOverscrollUpdate().
  float max_delta_ = 0.f;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_
