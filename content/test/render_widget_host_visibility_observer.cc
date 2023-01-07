// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/render_widget_host_visibility_observer.h"

namespace content {

RenderWidgetHostVisibilityObserver::RenderWidgetHostVisibilityObserver(
    RenderWidgetHostImpl* rwhi,
    bool expected_visibility_state)
    : expected_visibility_state_(expected_visibility_state),
      was_observed_(false),
      did_fail_(false),
      render_widget_(rwhi) {
  observation_.Observe(render_widget_.get());
}

RenderWidgetHostVisibilityObserver::~RenderWidgetHostVisibilityObserver() =
    default;

bool RenderWidgetHostVisibilityObserver::WaitUntilSatisfied() {
  if (!was_observed_)
    run_loop_.Run();
  observation_.Reset();
  return !did_fail_;
}

void RenderWidgetHostVisibilityObserver::RenderWidgetHostVisibilityChanged(
    RenderWidgetHost* widget_host,
    bool became_visible) {
  was_observed_ = true;
  did_fail_ = expected_visibility_state_ != became_visible;
  run_loop_.Quit();
}

void RenderWidgetHostVisibilityObserver::RenderWidgetHostDestroyed(
    RenderWidgetHost* widget_host) {
  DCHECK(observation_.IsObservingSource(widget_host));
  observation_.Reset();
}

}  // namespace content
