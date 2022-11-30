// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_web_frame_widget.h"

#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"

namespace blink {

void SimWebFrameWidget::DidBeginMainFrame() {
  // Notify the SimCompositor before calling the super, as the
  // overriden method will advance the lifecycle.
  sim_compositor_->DidBeginMainFrame();
  frame_test_helpers::TestWebFrameWidget::DidBeginMainFrame();
}

}  // namespace blink
