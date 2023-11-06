// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ForceDarkTest : public PageTestBase {
 protected:
  ForceDarkTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
    GetDocument().GetSettings()->SetPreferredColorScheme(
        mojom::blink::PreferredColorScheme::kDark);
  }
};

TEST_F(ForceDarkTest, ForcedColorScheme) {
  SetBodyInnerHTML(R"HTML(
    <div id="t1" style="color-scheme:initial"><span></span></div>
    <div id="t2" style="color-scheme:light"><span></span></div>
    <div id="t3" style="color-scheme:dark"><span></span></div>
    <div id="t4" style="color-scheme:light dark"><span></span></div>
    <div id="t5" style="color-scheme:only light"><span></span></div>
    <div id="t6" style="color-scheme:only dark"><span></span></div>
    <div id="t7" style="color-scheme:only light dark"><span></span></div>
    <div id="t8" style="color-scheme:inherit"><span></span></div>
  )HTML");

  struct TestCase {
    const char* id;
    bool expected_dark;
    bool expected_forced;
  };

  auto run_test = [&document = GetDocument()](const TestCase& test_case) {
    auto* element = document.getElementById(AtomicString(test_case.id));
    ASSERT_TRUE(element);

    const auto* style = element->GetComputedStyle();
    ASSERT_TRUE(style);
    EXPECT_EQ(test_case.expected_dark, style->DarkColorScheme())
        << "Element #" << test_case.id;
    EXPECT_EQ(test_case.expected_forced, style->ColorSchemeForced())
        << "Element #" << test_case.id;

    const auto* child_style = element->firstElementChild()->GetComputedStyle();
    ASSERT_TRUE(child_style);
    EXPECT_EQ(test_case.expected_dark, child_style->DarkColorScheme())
        << "Element #" << test_case.id << " > span";
    EXPECT_EQ(test_case.expected_forced, child_style->ColorSchemeForced())
        << "Element #" << test_case.id << " > span";
  };

  TestCase test_cases_preferred_dark[] = {
      {"t1", true, true},  {"t2", true, true},   {"t3", true, false},
      {"t4", true, false}, {"t5", false, false}, {"t6", true, false},
      {"t7", true, false}, {"t8", true, true},
  };

  for (const auto& test_case : test_cases_preferred_dark) {
    run_test(test_case);
  }

  GetDocument().GetSettings()->SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  UpdateAllLifecyclePhasesForTest();

  TestCase test_cases_preferred_light[] = {
      {"t1", true, true}, {"t2", true, true}, {"t3", true, true},
      {"t4", true, true}, {"t5", true, true}, {"t6", true, true},
      {"t7", true, true}, {"t8", true, true},
  };

  for (const auto& test_case : test_cases_preferred_light) {
    run_test(test_case);
  }
}

TEST_F(ForceDarkTest, ForcedColorSchemeInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <div id="force-light" style="color-scheme:only light">
      <div id="t1" style="color-scheme:dark"><span></span></div>
      <div id="t2" style="color-scheme:light"><span></span></div>
      <div id="t3" style="color-scheme:light"><span></span></div>
    </div>
  )HTML");

  struct TestCase {
    const char* id;
    bool expected_dark;
    bool expected_forced;
    bool expected_repaint;
  };

  auto run_test = [&document = GetDocument()](const TestCase& test_case) {
    auto* element = document.getElementById(AtomicString(test_case.id));
    ASSERT_TRUE(element);

    const auto* style = element->GetComputedStyle();
    ASSERT_TRUE(style);
    EXPECT_EQ(test_case.expected_dark, style->DarkColorScheme())
        << "Element #" << test_case.id;
    EXPECT_EQ(test_case.expected_forced, style->ColorSchemeForced())
        << "Element #" << test_case.id;
    EXPECT_EQ(test_case.expected_repaint,
              element->GetLayoutObject()->ShouldDoFullPaintInvalidation())
        << "Element #" << test_case.id;
  };

  ASSERT_TRUE(GetDocument().GetSettings()->GetForceDarkModeEnabled());
  GetDocument().GetSettings()->SetForceDarkModeEnabled(false);
  auto* t3 = GetDocument().getElementById(AtomicString("t3"));
  t3->SetInlineStyleProperty(CSSPropertyID::kColorScheme, "dark");
  GetDocument().UpdateStyleAndLayoutTree();

  TestCase test_cases_disable_force[] = {
      {"force-light", false, false, false},
      {"t1", true, false, false},
      {"t2", false, false, true},
      {"t3", true, false, true},
  };

  for (const TestCase& test_case : test_cases_disable_force) {
    run_test(test_case);
  }

  UpdateAllLifecyclePhasesForTest();
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  t3->SetInlineStyleProperty(CSSPropertyID::kColorScheme, "light");
  GetDocument().UpdateStyleAndLayoutTree();

  TestCase test_cases_enable_force[] = {
      {"force-light", false, false, false},
      {"t1", true, false, false},
      {"t2", true, true, true},
      {"t3", true, true, true},
  };

  for (const TestCase& test_case : test_cases_enable_force) {
    run_test(test_case);
  }
}

}  // namespace blink
