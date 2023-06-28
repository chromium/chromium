// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_filter_element.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SVGFilterElementSimTest : public SimTest {};

TEST_F(SVGFilterElementSimTest,
       FilterInvalidatedIfPrimitivesChangeDuringParsing) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  String document_text(R"HTML(
    <!doctype html>
    <div id="target" style="width: 100px; height: 100px; filter: url(#green)">
    </div>
    <svg><filter id="green"><feFlood flood-color="green"/></filter></svg>
  )HTML");
  const wtf_size_t cut_offset = document_text.Find("<feFlood");
  ASSERT_NE(cut_offset, kNotFound);

  main_resource.Write(document_text.Left(cut_offset));
  Compositor().BeginFrame();
  test::RunPendingTasks();

  const Element* target_element =
      GetDocument().getElementById(AtomicString("target"));
  const LayoutObject* target = target_element->GetLayoutObject();

  EXPECT_TRUE(target->StyleRef().HasFilter());
  ASSERT_FALSE(target->NeedsPaintPropertyUpdate());
  EXPECT_NE(nullptr, target->FirstFragment().PaintProperties()->Filter());

  main_resource.Complete(document_text.Right(cut_offset));

  ASSERT_TRUE(target->NeedsPaintPropertyUpdate());
}

}  // namespace blink
