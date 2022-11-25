// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class MathMLUseCountersTest : public SimTest {
 public:
  MathMLUseCountersTest() = default;

 protected:
  void LoadPage(const String& source) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(source);
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  void LoadPageWithDynamicMathML(const String& tagName, bool insertInBody) {
    StringBuilder source;
    source.Append(
        "<body>"
        "<script>"
        "let element = document.createElementNS("
        "'http://www.w3.org/1998/Math/MathML', '");
    source.Append(tagName);
    source.Append("');");
    if (insertInBody) {
      source.Append("document.body.appendChild(element);");
    }
    source.Append(
        "</script>"
        "</body>");
    LoadPage(source.ToString());
  }
};

TEST_F(MathMLUseCountersTest, MathMLUseCountersTest_NoMath) {
  // kMathML* counters not set for pages without MathML content.
  LoadPage("<body>Hello World!</body>");
  ASSERT_FALSE(GetDocument().IsUseCounted(WebFeature::kMathMLMathElement));
  ASSERT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kMathMLMathElementInDocument));
}

TEST_F(MathMLUseCountersTest, MathMLUseCountersTest_MinimalMath) {
  // kMathMLMath* counters set for a minimal page containing an empty math tag.
  LoadPage("<math></math>");
  ASSERT_TRUE(GetDocument().IsUseCounted(WebFeature::kMathMLMathElement));
  ASSERT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kMathMLMathElementInDocument));
}

TEST_F(MathMLUseCountersTest, MathMLUseCountersTest_HTMLAndBasicMath) {
  // kMathMLMath* counters set for a HTML page with some basic MathML formula.
  LoadPage(
      "<!DOCTYPE>"
      "<body>"
      "<p>"
      "<math><mfrac><msqrt><mn>2</mn></msqrt><mi>x</mi></mfrac></math>"
      "</p>"
      "</body>");
  ASSERT_TRUE(GetDocument().IsUseCounted(WebFeature::kMathMLMathElement));
  ASSERT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kMathMLMathElementInDocument));
}

TEST_F(MathMLUseCountersTest, MathMLUseCountersTest_DynamicMath) {
  // kMathMLMath* counters not set for a MathML element other that <math>.
  LoadPageWithDynamicMathML("mrow", true /* insertInBody */);
  ASSERT_FALSE(GetDocument().IsUseCounted(WebFeature::kMathMLMathElement));
  ASSERT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kMathMLMathElementInDocument));

  // Distinguish kMathMLMathElement and kMathMLMathElementInDocument
  LoadPageWithDynamicMathML("math", false /* insertInBody */);
  ASSERT_TRUE(GetDocument().IsUseCounted(WebFeature::kMathMLMathElement));
  ASSERT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kMathMLMathElementInDocument));
  LoadPageWithDynamicMathML("math", true /* insertInBody */);
  ASSERT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kMathMLMathElementInDocument));
}

}  // namespace blink
