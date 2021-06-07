// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"

#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class HTMLCanvasResourceTrackerTest : public RenderingTest {
 public:
  HTMLCanvasResourceTrackerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(HTMLCanvasResourceTrackerTest, AddCanvasElement) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<canvas id='canvas'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(GetDocument().getElementById("canvas"));
  auto* context = GetDocument().GetExecutionContext();
  for (auto entry :
       CanvasResourceTracker::For(context->GetIsolate())->GetResourceMap()) {
    EXPECT_EQ(canvas, entry.key);
    EXPECT_EQ(context, entry.value);
  }
}

}  // namespace blink
