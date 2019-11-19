// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_environment_variables.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

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
    frame.GetDocument()->body()->SetInnerHTMLFromString(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
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
        frame, StyleEnvironmentVariables::GetVariableName(name));
  }

  void SimulateNavigation() {
    const KURL& url = KURL(NullURL(), "https://www.example.com");
    GetDocument().GetFrame()->Loader().CommitNavigation(
        WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(), url),
        nullptr /* extra_data */);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }

  const String& GetRootVariableValue(UADefinedVariable name) {
    CSSVariableData* data =
        StyleEnvironmentVariables::GetRootInstance().ResolveVariable(
            StyleEnvironmentVariables::GetVariableName(name));
    EXPECT_NE(nullptr, data);
    return data->BackingStrings()[0];
  }
};

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_AfterLoad) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Change) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Change the variable value after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       DocumentVariable_Override_RemoveDocument) {
  // Set the variable globally.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the document variable.
  GetDocumentVariables().RemoveVariable(kVariableName);

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
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the global variable.
  StyleEnvironmentVariables::GetRootInstance().RemoveVariable(kVariableName);

  // Ensure that the document has not been invalidated.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Preset) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Remove) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  GetDocumentVariables().RemoveVariable(kVariableName);

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
  auto new_page = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  InitializeTestPageWithVariableNamed(new_page->GetFrame(), kVariableName);

  // Create an empty page that does not use the variable.
  auto empty_page = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  empty_page->GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // The first two pages should be invalidated and the empty one should not.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(new_page->GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(empty_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, MultiDocumentInvalidation_FromDocument) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Create a second page that uses the variable.
  auto new_page = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  InitializeTestPageWithVariableNamed(new_page->GetFrame(), kVariableName);

  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Only the first document should be invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(new_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, NavigateToClear) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Simulate a navigation to clear the variables.
  SimulateNavigation();
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has no background color.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kNoColor, target->ComputedStyleRef().VisitedDependentColor(
                          GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_AfterLoad) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Change) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Change the variable value after we have loaded the page.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
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

  EXPECT_EQ(
      nullptr,
      StyleEnvironmentVariables::GetRootInstance().ResolveVariable("test"));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Preset) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Remove) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  StyleEnvironmentVariables::GetRootInstance().RemoveVariable(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Check that the element does not have the background color any more.
  EXPECT_NE(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       DISABLED_PrintExpectedVariableNameHashes) {
  const UADefinedVariable variables[] = {
      UADefinedVariable::kSafeAreaInsetTop,
      UADefinedVariable::kSafeAreaInsetLeft,
      UADefinedVariable::kSafeAreaInsetRight,
      UADefinedVariable::kSafeAreaInsetBottom};
  for (const auto& variable : variables) {
    const AtomicString name =
        StyleEnvironmentVariables::GetVariableName(variable);
    printf("0x%x\n",
           DocumentStyleEnvironmentVariables::GenerateHashFromName(name));
  }
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

}  // namespace blink
