// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_image_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class CSSImageValueTest : public SimTest {};

TEST_F(CSSImageValueTest, BlockPotentiallyDanglingMarkup) {
  SimRequest main_resource("https://example.com", "text/html");

  LoadURL("https://example.com");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <table id="t1" background="ht
    tps://example.com/y<ay?foo"><td>XXX</td></table>
    <table id="t2" background="ht
    tps://example.com/y<ay?bar#boo"><td>XXX</td></table>
  )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  auto* t1 = GetDocument().getElementById(AtomicString("t1"));
  ImageResourceContent* content1 =
      t1->ComputedStyleRef().BackgroundLayers().GetImage()->CachedImage();
  ASSERT_TRUE(content1);
  EXPECT_TRUE(content1->ErrorOccurred());

  auto* t2 = GetDocument().getElementById(AtomicString("t2"));
  ImageResourceContent* content2 =
      t2->ComputedStyleRef().BackgroundLayers().GetImage()->CachedImage();
  ASSERT_TRUE(content2);
  EXPECT_TRUE(content2->ErrorOccurred());
}

}  // namespace blink
