// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_WEB_FRAME_WIDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_WEB_FRAME_WIDGET_H_

#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"

namespace blink {

class SimCompositor;

class SimWebFrameWidget : public frame_test_helpers::TestWebFrameWidget {
 public:
  template <typename... Args>
  SimWebFrameWidget(SimCompositor* compositor, Args&&... args)
      : frame_test_helpers::TestWebFrameWidget(std::forward<Args>(args)...),
        sim_compositor_(compositor) {}

  // WidgetBaseClient overrides.
  void DidBeginMainFrame() override;

 private:
  SimCompositor* const sim_compositor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_WEB_FRAME_WIDGET_H_
