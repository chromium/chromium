// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().getElementById(AtomicString("canvas")));
  auto* context = GetDocument().GetExecutionContext();
  const auto& resource_map =
      CanvasResourceTracker::For(context->GetIsolate())->GetResourceMap();
  // The map may hold more than a single entry as CanvasResourceTracker is
  // instantiated per v8::Isolate which is reused across tests.
  const auto it = resource_map.find(canvas);
  EXPECT_NE(resource_map.end(), it);
  EXPECT_EQ(context, it->value);
}

}  // namespace blink
