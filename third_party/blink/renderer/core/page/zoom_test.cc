// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class FractionalZoomSimTest : public SimTest {};

TEST_F(FractionalZoomSimTest, CheckCSSMediaQueryWidthEqualsWindowInnerWidth) {
  WebView().MainFrameWidget()->Resize(WebSize(1081, 1921));

  // 1081/2.75 = 393.091
  // 1081/2.00 = 540.500
  // 1081/1.50 = 720.667
  std::vector<float> factors = {2.75f, 2.00f, 1.50f};
  for (auto factor : factors) {
    WebView().SetZoomFactorForDeviceScaleFactor(factor);
    EXPECT_EQ(GetDocument().View()->ViewportSizeForMediaQueries().Width(),
              GetDocument().GetFrame()->DomWindow()->innerWidth());
  }
}

}  // namespace blink
