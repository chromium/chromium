// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_environment_variables.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

static const char kVariableName[] = "test";

// red
static const Color kTestColorRed = Color(255, 0, 0);
static const char kVariableTestColor[] = "red";

// blue
static const Color kAltTestColor = Color(0, 0, 255);
static const char kVariableAltTestColor[] = "blue";

// no set
static const Color kNoColor = Color(0, 0, 0, 0);

static const char kSafeAreaInsetExpectedDefault[] = "0px";

}  // namespace

class StyleEnvironmentVariablesTest : public PageTestBase {
 public:
  void TearDown() override {
    StyleEnvironmentVariables::GetRootInstance().ClearForTesting();
  }

  DocumentStyleEnvironmentVariables& GetDocumentVariables() {
    return GetStyleEngine().EnsureEnvironmentVariables();
  }

  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    // Sets the inner html and runs the document lifecycle.
    frame.GetDocument()->body()->setInnerHTML(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhasesForTest();
  }

  void InitializeTestPageWithVariableNamed(LocalFrame& frame,
                                           const String& name) {
    InitializeWithHTML(frame,
                       "<style>"
                       "  #target { background-color: env(" +
                           name +
                           "); }"
                           "</style>"
                           "<div>"
                           "  <div id=target></div>"
                           "</div>");
  }

  void InitializeTestPageWithVariableNamed(LocalFrame& frame,
                                           const UADefinedVariable name) {
    InitializeTestPageWithVariableNamed(
        frame, StyleEnvironmentVariables::GetVariableName(
                   name, /*feature_context=*/nullptr));
  }

  void SimulateNavigation() {
    const KURL& url = KURL(NullURL(), "https://www.example.com");
    GetDocument().GetFrame()->Loader().CommitNavigation(
        WebNavigationParams::CreateWithEmptyHTMLForTesting(url),
        nullptr /* extra_data */);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }

  String GetRootVariableValue(UADefinedVariable name) {
    CSSVariableData* data =
        StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
            StyleEnvironmentVariables::GetVariableName(
                name, /*feature_context=*/nullptr),
            {});
    EXPECT_NE(nullptr, data);
    return data->Serialize();
  }

  void SetVariableOnRoot(const char* name, const String& value) {
    StyleEnvironmentVariables::GetRootInstance().SetVariable(AtomicString(name),
                                                             value);
  }

  void RemoveVariableOnRoot(const char* name) {
    StyleEnvironmentVariables::GetRootInstance().RemoveVariable(
        AtomicString(name));
  }

  void SetVariableOnDocument(const char* name, const String& value) {
    GetDocumentVariables().SetVariable(AtomicString(name), value);
  }

  void RemoveVariableOnDocument(const char* name) {
    GetDocumentVariables().RemoveVariable(AtomicString(name));
  }

  void SetTwoDimensionalVariableOnRoot(UADefinedTwoDimensionalVariable variable,
                                       unsigned first_dimension,
                                       unsigned second_dimension,
                                       const String& value) {
    StyleEnvironmentVariables::GetRootInstance().SetVariable(
        variable, first_dimension, second_dimension, value, nullptr);
  }
};

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_AfterLoad) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  SetVariableOnDocument(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Change) {
  SetVariableOnDocument(kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Change the variable value after we have loaded the page.
  SetVariableOnDocument(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       DocumentVariable_Override_RemoveDocument) {
  // Set the variable globally.
  SetVariableOnRoot(kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  SetVariableOnDocument(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the document variable.
  RemoveVariableOnDocument(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the global
  // variable.
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Override_RemoveGlobal) {
  // Set the variable globally.
  SetVariableOnRoot(kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  SetVariableOnDocument(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the global variable.
  RemoveVariableOnRoot(kVariableName);

  // Ensure that the document has not been invalidated.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Preset) {
  SetVariableOnDocument(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Remove) {
  SetVariableOnDocument(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  RemoveVariableOnDocument(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element does not have the background color any more.
  EXPECT_NE(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, MultiDocumentInvalidation_FromRoot) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Create a second page that uses the variable.
  auto new_page = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  InitializeTestPageWithVariableNamed(new_page->GetFrame(), kVariableName);

  // Create an empty page that does not use the variable.
  auto empty_page = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  empty_page->GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  SetVariableOnRoot(kVariableName, kVariableTestColor);

  // The first two pages should be invalidated and the empty one should not.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(new_page->GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(empty_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, MultiDocumentInvalidation_FromDocument) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Create a second page that uses the variable.
  auto new_page = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  InitializeTestPageWithVariableNamed(new_page->GetFrame(), kVariableName);

  SetVariableOnDocument(kVariableName, kVariableTestColor);

  // Only the first document should be invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(new_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, NavigateToClear) {
  SetVariableOnDocument(kVariableName, kVariableTestColor);

  // Simulate a navigation to clear the variables.
  SimulateNavigation();
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has no background color.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kNoColor, target->ComputedStyleRef().VisitedDependentColor(
                          GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_AfterLoad) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  SetVariableOnRoot(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Change) {
  SetVariableOnRoot(kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Change the variable value after we have loaded the page.
  SetVariableOnRoot(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_DefaultsPresent) {
  EXPECT_EQ(kSafeAreaInsetExpectedDefault,
            GetRootVariableValue(UADefinedVariable::kSafeAreaInsetTop));
  EXPECT_EQ(kSafeAreaInsetExpectedDefault,
            GetRootVariableValue(UADefinedVariable::kSafeAreaInsetLeft));
  EXPECT_EQ(kSafeAreaInsetExpectedDefault,
            GetRootVariableValue(UADefinedVariable::kSafeAreaInsetBottom));
  EXPECT_EQ(kSafeAreaInsetExpectedDefault,
            GetRootVariableValue(UADefinedVariable::kSafeAreaInsetRight));

  EXPECT_EQ(nullptr,
            StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
                AtomicString("test"), {}));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Preset) {
  SetVariableOnRoot(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Remove) {
  SetVariableOnRoot(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  RemoveVariableOnRoot(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element does not have the background color any more.
  EXPECT_NE(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_IgnoreMediaControls) {
  InitializeWithHTML(GetFrame(), "<video controls />");

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSEnvironmentVariable));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_InvalidProperty) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSEnvironmentVariable));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_NoVariable) {
  InitializeWithHTML(GetFrame(), "");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSEnvironmentVariable));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetBottom) {
  InitializeTestPageWithVariableNamed(GetFrame(),
                                      UADefinedVariable::kSafeAreaInsetBottom);

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom));
}

// TODO(https://crbug.com/1430288) remove after data collected (end of '23)
TEST_F(StyleEnvironmentVariablesTest,
       RecordUseCounter_ViewportFitCoverOrSafeAreaInsetBottom) {
  InitializeWithHTML(GetFrame(), "");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kViewportFitCoverOrSafeAreaInsetBottom));
  InitializeTestPageWithVariableNamed(GetFrame(),
                                      UADefinedVariable::kSafeAreaInsetBottom);
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kViewportFitCoverOrSafeAreaInsetBottom));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetLeft) {
  InitializeTestPageWithVariableNamed(GetFrame(),
                                      UADefinedVariable::kSafeAreaInsetLeft);

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetRight) {
  InitializeTestPageWithVariableNamed(GetFrame(),
                                      UADefinedVariable::kSafeAreaInsetRight);

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetTop) {
  InitializeTestPageWithVariableNamed(GetFrame(),
                                      UADefinedVariable::kSafeAreaInsetTop);

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop));
}

TEST_F(StyleEnvironmentVariablesTest, KeyboardInset_AfterLoad) {
  // This test asserts that the keyboard inset environment variables are loaded
  // by default.
  CSSVariableData* data =
      StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
          StyleEnvironmentVariables::GetVariableName(
              UADefinedVariable::kKeyboardInsetTop,
              /*feature_context=*/nullptr),
          {});
  EXPECT_TRUE(data);
  data = StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kKeyboardInsetLeft, /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  data = StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kKeyboardInsetBottom,
          /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  data = StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kKeyboardInsetRight, /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  data = StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kKeyboardInsetWidth, /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  data = StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kKeyboardInsetHeight,
          /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
}

TEST_F(StyleEnvironmentVariablesTest, TwoDimensionalVariables_BasicResolve) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-top 1 0");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 1, 0, "red");

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, TwoDimensionalVariables_UpdateValue) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-top 1 0");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 1, 0, "red");

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 1, 0, "blue");
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       TwoDimensionalVariables_UndefinedFallsBack) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents(
      "viewport-segment-width 10 20, env(viewport-segment-width 0 0, blue)");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentWidth, 1, 1, "red");

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the fallback.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       TwoDimensionalVariables_IncorrectDimensionsFallsBack) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-width 0 0 0 0, blue");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentWidth, 0, 0, "red");

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the fallback.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       TwoDimensionalVariables_NormalVariableWithDimensionFallsBack) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("safe-area-inset-left 0, blue");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetVariableOnRoot("safe-area-inset-left", "red");

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the fallback.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       TwoDimensionalVariables_NegativeIndicesInvalid) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-top -1 -1, blue");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 0, 0, "red");
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 1, 1, "red");

  // Document should not have been invalidated since the value was a parse
  // error and viewport-segment-left is not referenced.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Check that the element has no cascaded background color.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kNoColor, target->ComputedStyleRef().VisitedDependentColor(
                          GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       TwoDimensionalVariables_NonCommaAfterIndexInvalid) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-left 1 1 ident");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentLeft, 1, 1, "red");

  // Document should not have been invalidated since the value was a parse
  // error and viewport-segment-left is not referenced.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Check that the element has no cascaded background color.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kNoColor, target->ComputedStyleRef().VisitedDependentColor(
                          GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       TwoDimensionalVariables_NonIntegerIndicesInvalid) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-top 0.5 0.5, blue");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 0, 0, "red");
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 1, 1, "red");

  // Document should not have been invalidated since the value was a parse
  // error and viewport-segment-left is not referenced.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Check that the element has no cascaded background color.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kNoColor, target->ComputedStyleRef().VisitedDependentColor(
                          GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       TwoDimensionalVariables_NoIndicesFallsBack) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-height, blue");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentTop, 0, 0, "red");

  // Document should not have been invalidated since the wrong dimensions can
  // never resolve (and thus the variable has not been 'seen').
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Check that the element has the background color provided by the fallback.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, TwoDimensionalVariables_Removal) {
  ScopedViewportSegmentsForTest scoped_feature(true);
  String env_contents("viewport-segment-height 0 0, blue");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);
  SetTwoDimensionalVariableOnRoot(
      UADefinedTwoDimensionalVariable::kViewportSegmentHeight, 0, 0, "red");

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  RemoveVariableOnRoot("viewport-segment-height");
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the fallback.
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(StyleEnvironmentVariablesTest, TitlebarArea_AfterLoad) {
  // This test asserts that the titlebar area environment variables should be
  // loaded when UpdateWindowControlsOverlay is invoked in LocalFrame for PWAs
  // with display_override "window-controls-overlay".

  // Simulate browser sending the titlebar area bounds.
  GetFrame().UpdateWindowControlsOverlay(gfx::Rect(0, 0, 100, 10));
  String env_contents("titlebar-area-x");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);

  // Validate the data is set.
  DocumentStyleEnvironmentVariables& vars =
      GetDocument().GetStyleEngine().EnsureEnvironmentVariables();

  CSSVariableData* data =
      vars.ResolveVariable(StyleEnvironmentVariables::GetVariableName(
                               UADefinedVariable::kTitlebarAreaX,
                               /*feature_context=*/nullptr),
                           {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "0px");
  data = vars.ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kTitlebarAreaY, /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "0px");
  data = vars.ResolveVariable(StyleEnvironmentVariables::GetVariableName(
                                  UADefinedVariable::kTitlebarAreaWidth,
                                  /*feature_context=*/nullptr),
                              {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "100px");
  data = vars.ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kTitlebarAreaHeight, /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "10px");
}

TEST_F(StyleEnvironmentVariablesTest, TitlebarArea_AfterNavigation) {
  // This test asserts that the titlebar area environment variables should be
  // set after a navigation for PWAs with display_override
  // "window-controls-overlay".

  // Simulate browser sending the titlebar area bounds.
  GetFrame().UpdateWindowControlsOverlay(gfx::Rect(0, 0, 100, 10));
  String env_contents("titlebar-area-x");
  InitializeTestPageWithVariableNamed(GetFrame(), env_contents);

  SimulateNavigation();

  // Validate the data is set after navigation.
  DocumentStyleEnvironmentVariables& vars =
      GetDocument().GetStyleEngine().EnsureEnvironmentVariables();

  CSSVariableData* data =
      vars.ResolveVariable(StyleEnvironmentVariables::GetVariableName(
                               UADefinedVariable::kTitlebarAreaX,
                               /*feature_context=*/nullptr),
                           {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "0px");
  data = vars.ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kTitlebarAreaY, /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "0px");
  data = vars.ResolveVariable(StyleEnvironmentVariables::GetVariableName(
                                  UADefinedVariable::kTitlebarAreaWidth,
                                  /*feature_context=*/nullptr),
                              {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "100px");
  data = vars.ResolveVariable(
      StyleEnvironmentVariables::GetVariableName(
          UADefinedVariable::kTitlebarAreaHeight, /*feature_context=*/nullptr),
      {});
  EXPECT_TRUE(data);
  EXPECT_EQ(data->Serialize(), "10px");
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
