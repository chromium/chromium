// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_image_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
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
  const StyleImage* image1 =
      t1->ComputedStyleRef().BackgroundLayers().GetImage();
  ASSERT_TRUE(image1);
  EXPECT_TRUE(image1->ErrorOccurred());

  auto* t2 = GetDocument().getElementById(AtomicString("t2"));
  const StyleImage* image2 =
      t2->ComputedStyleRef().BackgroundLayers().GetImage();
  ASSERT_TRUE(image2);
  EXPECT_TRUE(image2->ErrorOccurred());
}

// Ensure that CSSImageValue preserves the potentially_dangling_markup flag
// when cloned or used via CSS Typed OM.
// Canonicalization of the URL string removes the newlines that trigger the
// dangling markup flag, so we must ensure it is manually propagated during
// these operations.
TEST_F(CSSImageValueTest, PreserveDanglingMarkupFlagInTypedOM) {
  SimRequest main_resource("https://example.com/index.html", "text/html");
  // If the bug were present, the dangling markup flag would be dropped and
  // this URL would be requested successfully. We mock it to allow the fetch
  // to proceed in the failure case, so we can verify ErrorOccurred().
  SimRequest dangling_resource("https://example.com/exfil%3Csecret",
                               "image/png");

  LoadURL("https://example.com/index.html");

  // Step 1: Set a dangling-markup URL on a hidden carrier element.
  main_resource.Complete(R"HTML(
    <!doctype html>
    <table id="carrier" style="display:none" background="/exfil
<secret"></table>
    <div id="target" style="width:100px; height:100px"></div>
  )HTML");

  test::RunPendingTasks();

  // Force style recalc for carrier.
  GetDocument().UpdateStyleAndLayoutTree();

  // 2. Round-trip the value through CSS Typed OM via JS.
  // This triggers internal cloning and conversion logic.
  MainFrame().ExecuteScript(WebScriptSource(R"JS(
    {
      const carrier = document.getElementById('carrier');
      const target = document.getElementById('target');
      // Retrieve the value (triggers cloning/resolution).
      const val = carrier.computedStyleMap().get('background-image');
      // Apply the value to another element.
      target.attributeStyleMap.set('background-image', val);
    }
  )JS"));

  // 3. Force style recalc and resource load for the target element.
  GetDocument().UpdateStyleAndLayoutTree();
  if (Compositor().NeedsBeginFrame()) {
    Compositor().BeginFrame();
  }

  // 4. Verify that the fetch was correctly BLOCKED (ErrorOccurred() == true).
  auto* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  const StyleImage* target_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  ASSERT_TRUE(target_image) << "target_image should not be null";
  EXPECT_TRUE(target_image->ErrorOccurred());
}

}  // namespace blink
