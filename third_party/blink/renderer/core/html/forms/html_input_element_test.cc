// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"

#include <memory>

#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_wheel_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/file_input_type.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/spin_button_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using ::testing::Truly;

namespace blink {

namespace {

class PasswordResetChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD(void,
              PasswordFieldReset,
              (HTMLInputElement & element),
              (override));
};

class HTMLInputElementTestChromeClient : public EmptyChromeClient {
 public:
  gfx::Rect LocalRootToScreenDIPs(const gfx::Rect& local_root_rect,
                                  const LocalFrameView* view) const override {
    return view->GetPage()->GetVisualViewport().RootFrameToViewport(
        local_root_rect);
  }
};

}  // namespace

class HTMLInputElementTest : public PageTestBase {
 protected:
  void SetUp() override {
    auto* chrome_client =
        MakeGarbageCollected<HTMLInputElementTestChromeClient>();
    SetupPageWithClients(chrome_client);
  }

  HTMLInputElement& TestElement() {
    Element* element = GetDocument().getElementById(AtomicString("test"));
    DCHECK(element);
    return To<HTMLInputElement>(*element);
  }
};

TEST_F(HTMLInputElementTest, FilteredDataListOptionsNoList) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().empty());

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test list=dl1><datalist id=dl1></datalist>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().empty());
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsContain) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test value=BC list=dl2>"
      "<datalist id=dl2>"
      "<option>AbC DEF</option>"
      "<option>VAX</option>"
      "<option value=ghi>abc</option>"  // Match to label, not value.
      "</datalist>");
  auto options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(2u, options.size());
  EXPECT_EQ("AbC DEF", options[0]->value().Utf8());
  EXPECT_EQ("ghi", options[1]->value().Utf8());

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test value=i list=dl2>"
      "<datalist id=dl2>"
      "<option>I</option>"
      "<option>&#x0130;</option>"  // LATIN CAPITAL LETTER I WITH DOT ABOVE
      "<option>&#xFF49;</option>"  // FULLWIDTH LATIN SMALL LETTER I
      "</datalist>");
  options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(2u, options.size());
  EXPECT_EQ("I", options[0]->value().Utf8());
  EXPECT_EQ(0x0130, options[1]->value()[0]);
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsForMultipleEmail) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <input id=test value='foo@example.com, tkent' list=dl3 type=email
    multiple>
    <datalist id=dl3>
    <option>keishi@chromium.org</option>
    <option>tkent@chromium.org</option>
    </datalist>
  )HTML");
  auto options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(1u, options.size());
  EXPECT_EQ("tkent@chromium.org", options[0]->value().Utf8());
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsDynamicContain) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <input id=test value='40m auto reel' list=dl4>
    <datalist id=dl4>
    <option>Hozelock 10m Mini Auto Reel - 2485</option>
    <option>Hozelock Auto Reel 20m - 2401</option>
    <option>Hozelock Auto Reel 30m - 2403</option>
    <option>Hozelock Auto Reel 40m - 2595</option>
    </datalist>
  )HTML");
  auto options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(1u, options.size());
  EXPECT_EQ("Hozelock Auto Reel 40m - 2595", options[0]->value().Utf8());

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <input id=test value='autoreel' list=dl4>
    <datalist id=dl4>
    <option>Hozelock 10m Mini Auto Reel - 2485</option>
    <option>Hozelock Auto Reel 20m - 2401</option>
    <option>Hozelock Auto Reel 30m - 2403</option>
    <option>Hozelock Auto Reel 40m - 2595</option>
    </datalist>
  )HTML");
  options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(4u, options.size());
  EXPECT_EQ("Hozelock 10m Mini Auto Reel - 2485", options[0]->value().Utf8());
  EXPECT_EQ("Hozelock Auto Reel 20m - 2401", options[1]->value().Utf8());
  EXPECT_EQ("Hozelock Auto Reel 30m - 2403", options[2]->value().Utf8());
  EXPECT_EQ("Hozelock Auto Reel 40m - 2595", options[3]->value().Utf8());
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsCaseFoldingSharpS) {
  // Datalist option has Eszett ("ß"), and we want to match it with "ß" input.
  // The bug was that typing "ß" (8-bit) matched the option, but copy-pasting
  // "ß" (16-bit) did not (see crbug.com/493179860 for more details).

  // Case A (Simulating Typing): Input is "ß" (8-bit), Option is "ß" (8-bit).
  // They both fold to "ß" (pre-fix) or both to "ss" (post-fix), so they match.
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <input id=test value="&#xDF;" list=dl_sharps>
    <datalist id=dl_sharps>
      <option>&#xDF;</option>
    </datalist>
  )HTML");
  auto options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(1u, options.size());
  EXPECT_EQ(0xDF, options[0]->value()[0]);

  // Case B (Simulating Pasting): Input is "ß" (forced 16-bit), Option is "ß"
  // (8-bit). Previously, 16-bit input folded to "ss" but 8-bit option folded to
  // "ß", resulting in no match. With the fix, both fold to "ss" and match.
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <input id=test list=dl_sharps2>
    <datalist id=dl_sharps2>
      <option>&#xDF;</option>
    </datalist>
  )HTML");
  const UChar sharps_16bit[] = {0xDF, 0};
  TestElement().SetValue(String(sharps_16bit));
  options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(1u, options.size());
  EXPECT_EQ(0xDF, options[0]->value()[0]);
}

TEST_F(HTMLInputElementTest, create) {
  auto* input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());

  input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByParser(&GetDocument()));
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  input->ParserSetAttributes(Vector<Attribute, kAttributePrealloc>());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, NoAssertWhenMovedInNewDocument) {
  ScopedNullExecutionContext execution_context;
  auto* document_without_frame =
      Document::CreateForTest(execution_context.GetExecutionContext());
  EXPECT_EQ(nullptr, document_without_frame->GetPage());
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document_without_frame);
  html->AppendChild(
      MakeGarbageCollected<HTMLBodyElement>(*document_without_frame));

  // Create an input element with type "range" inside a document without frame.
  To<HTMLBodyElement>(html->firstChild())
      ->SetInnerHTMLWithoutTrustedTypes("<input type='range' />");
  document_without_frame->AppendChild(html);

  auto page_holder = std::make_unique<DummyPageHolder>();
  auto& document = page_holder->GetDocument();
  EXPECT_NE(nullptr, document.GetPage());

  // Put the input element inside a document with frame.
  document.body()->AppendChild(document_without_frame->body()->firstChild());

  // Remove the input element and all refs to it so it gets deleted before the
  // document.
  // The assert in |EventHandlerRegistry::updateEventHandlerTargets()| should
  // not be triggered.
  document.body()->RemoveChild(document.body()->firstChild());
}

TEST_F(HTMLInputElementTest, DefaultToolTip) {
  auto* input_without_form =
      MakeGarbageCollected<HTMLInputElement>(GetDocument());
  input_without_form->SetBooleanAttribute(html_names::kRequiredAttr, true);
  GetDocument().body()->AppendChild(input_without_form);
  EXPECT_EQ("<<ValidationValueMissing>>", input_without_form->DefaultToolTip());

  auto* form = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  GetDocument().body()->AppendChild(form);
  auto* input_with_form = MakeGarbageCollected<HTMLInputElement>(GetDocument());
  input_with_form->SetBooleanAttribute(html_names::kRequiredAttr, true);
  form->AppendChild(input_with_form);
  EXPECT_EQ("<<ValidationValueMissing>>", input_with_form->DefaultToolTip());

  form->SetBooleanAttribute(html_names::kNovalidateAttr, true);
  EXPECT_EQ(String(), input_with_form->DefaultToolTip());
}

// crbug.com/589838
TEST_F(HTMLInputElementTest, ImageTypeCrash) {
  auto* input = MakeGarbageCollected<HTMLInputElement>(GetDocument());
  input->setAttribute(html_names::kTypeAttr, AtomicString("image"));
  input->EnsureFallbackContent();
  // Make sure ensurePrimaryContent() recreates UA shadow tree, and updating
  // |value| doesn't crash.
  input->EnsurePrimaryContent();
  input->setAttribute(html_names::kValueAttr, AtomicString("aaa"));
}

TEST_F(HTMLInputElementTest, RadioKeyDownDCHECKFailure) {
  // crbug.com/697286
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input type=radio name=g><input type=radio name=g>");
  auto& radio1 = To<HTMLInputElement>(*GetDocument().body()->firstChild());
  auto& radio2 = To<HTMLInputElement>(*radio1.nextSibling());
  radio1.Focus();
  // Make layout-dirty.
  radio2.setAttribute(html_names::kStyleAttr, AtomicString("position:fixed"));
  KeyboardEventInit* init = KeyboardEventInit::Create();
  init->setKey(keywords::kArrowRight);
  radio1.DefaultEventHandler(
      *MakeGarbageCollected<KeyboardEvent>(event_type_names::kKeydown, init));
  EXPECT_EQ(GetDocument().ActiveElement(), &radio2);
}

TEST_F(HTMLInputElementTest, DateTimeChooserSizeParamRespectsScale) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  GetDocument().View()->GetFrame().GetPage()->GetVisualViewport().SetScale(2.f);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input type='date' style='width:200px;height:50px' />");
  UpdateAllLifecyclePhasesForTest();
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());

  DateTimeChooserParameters params;
  bool success = input->SetupDateTimeChooserParameters(params);
  EXPECT_TRUE(success);
  EXPECT_EQ(InputType::Type::kDate, params.type);
  EXPECT_EQ(gfx::Rect(16, 16, 400, 100), params.anchor_rect_in_screen);
}

TEST_F(HTMLInputElementTest, StepDownOverflow) {
  auto* input = MakeGarbageCollected<HTMLInputElement>(GetDocument());
  input->setAttribute(html_names::kTypeAttr, AtomicString("date"));
  input->setAttribute(html_names::kMinAttr, AtomicString("2010-02-10"));
  input->setAttribute(html_names::kStepAttr,
                      AtomicString("9223372036854775556"));
  // InputType::ApplyStep() should not pass an out-of-range value to
  // SetValueAsDecimal, and blink::MsToYear() should not cause a DCHECK failure.
  input->stepDown(1, ASSERT_NO_EXCEPTION);
}

TEST_F(HTMLInputElementTest, StepDownDefaultToMin) {
  AtomicString min_attr_value("7");

  auto* input = MakeGarbageCollected<HTMLInputElement>(GetDocument());
  input->setAttribute(html_names::kTypeAttr, AtomicString("number"));
  input->setAttribute(html_names::kMinAttr, min_attr_value);

  EXPECT_TRUE(input->Value().empty());

  input->stepDown(1, ASSERT_NO_EXCEPTION);

  // stepDown() should default to min value when the input has no initial value.
  EXPECT_EQ(min_attr_value, input->Value());
}

TEST_F(HTMLInputElementTest, CheckboxHasNoShadowRoot) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input type='checkbox' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, ChangingInputTypeCausesShadowRootToBeCreated) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input type='checkbox' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  EXPECT_NE(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, RepaintAfterClearingFile) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input type='file' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());

  FileChooserFileInfoList files;
  files.push_back(CreateFileChooserFileInfoNative("/native/path/native-file",
                                                  "display-name"));
  auto* execution_context = MakeGarbageCollected<NullExecutionContext>();
  FileList* list = FileInputType::CreateFileList(*execution_context, files,
                                                 base::FilePath());
  ASSERT_TRUE(list);
  EXPECT_EQ(1u, list->length());

  input->setFiles(list);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(input->GetLayoutObject());
  EXPECT_FALSE(input->GetLayoutObject()->ShouldCheckForPaintInvalidation());

  input->SetValue("");
  GetDocument().UpdateStyleAndLayoutTree();

  ASSERT_TRUE(input->GetLayoutObject());
  EXPECT_TRUE(input->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  execution_context->NotifyContextDestroyed();
}

TEST_F(HTMLInputElementTest, UpdateTypeDcheck) {
  Document& doc = GetDocument();
  // Removing <body> is required to reproduce the issue.
  doc.body()->remove();
  Element* input = doc.CreateRawElement(html_names::kInputTag);
  doc.documentElement()->appendChild(input);
  input->Focus();
  input->setAttribute(html_names::kTypeAttr, AtomicString("radio"));
  // Test succeeds if the above setAttribute() didn't trigger a DCHECK failure
  // in Document::UpdateFocusAppearanceAfterLayout().
}

TEST_F(HTMLInputElementTest, LazilyCreateShadowTree) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<input/>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);
  EXPECT_FALSE(IsShadowHost(*input));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(IsShadowHost(*input));
}

TEST_F(HTMLInputElementTest, LazilyCreateShadowTreeWithPlaceholder) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input placeholder='x'/>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);
  EXPECT_FALSE(IsShadowHost(*input));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(IsShadowHost(*input));
}

TEST_F(HTMLInputElementTest, LazilyCreateShadowTreeWithValue) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<input value='x'/>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);
  EXPECT_FALSE(IsShadowHost(*input));
}

// Tests that HasBeenPasswordField() remains true as the form control type
// changes, until it changes to a non-text form control type.
TEST_F(HTMLInputElementTest, HasBeenPasswordField) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<input>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);
  EXPECT_FALSE(input->HasBeenPasswordField());
  input->setType(input_type_names::kPassword);
  EXPECT_TRUE(input->HasBeenPasswordField());
  input->setType(input_type_names::kText);
  EXPECT_TRUE(input->HasBeenPasswordField());
  input->setType(input_type_names::kNumber);
  EXPECT_TRUE(input->HasBeenPasswordField());
  input->setType(input_type_names::kCheckbox);
  EXPECT_FALSE(input->HasBeenPasswordField());

  // MaybeSetHasBeenPasswordField() only has an effect on IsTextType() elements.
  input->setType(input_type_names::kUrl);
  EXPECT_FALSE(input->HasBeenPasswordField());
  input->MaybeSetHasBeenPasswordField();
  EXPECT_TRUE(input->HasBeenPasswordField());
  input->setType(input_type_names::kRadio);
  EXPECT_FALSE(input->HasBeenPasswordField());
  input->MaybeSetHasBeenPasswordField();
  EXPECT_FALSE(input->HasBeenPasswordField());
}

TEST_F(HTMLInputElementTest, HeuristicCustomPasswordDetectionCSS) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=text value='abc'>");
  auto& input = TestElement();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(input.HasBeenHeuristicCustomPasswordCSS());

  // Applying -webkit-text-security should trigger detection.
  input.setAttribute(html_names::kStyleAttr,
                     AtomicString("-webkit-text-security: disc;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordCSS());

  // Removing the style should not clear the "has ever been" state.
  input.removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordCSS());
}

TEST_F(HTMLInputElementTest, HeuristicCustomPasswordDetectionCSSFromAttribute) {
  // Detection during parsing/attribute setting.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=text value='abc' style='-webkit-text-security: "
      "disc;'>");
  UpdateAllLifecyclePhasesForTest();
  auto& input = TestElement();
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordCSS());
}

TEST_F(HTMLInputElementTest, HeuristicCustomPasswordDetectionJS) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=text>");
  auto& input = TestElement();

  // Programmatic value change to a masked pattern.
  input.SetValue("****a");
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordJS());
}

TEST_F(HTMLInputElementTest, HeuristicCustomPasswordDetectionJSFromAttribute) {
  // Detection during parsing/attribute setting.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=text value='****a'>");
  UpdateAllLifecyclePhasesForTest();
  auto& input = TestElement();
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordJS());
}

TEST_F(HTMLInputElementTest, HeuristicCustomPasswordFieldJSTypeChange) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=text>");
  auto& input = TestElement();

  // Programmatic value change to a masked pattern.
  input.SetValue("****a");
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordJS());

  // Change type to a non-text control (e.g., checkbox). This should clear the
  // heuristic state to align with native behavior.
  input.setType(input_type_names::kCheckbox);
  EXPECT_FALSE(input.HasBeenHeuristicCustomPasswordJS());

  // Change type back to text. The heuristic should be re-evaluated. Since the
  // value "•••••" is still there, it should be identified again.
  input.setType(input_type_names::kText);
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordJS());
}

TEST_F(HTMLInputElementTest, HeuristicCustomPasswordFieldCSSTypeChange) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=text value='abc'>");
  auto& input = TestElement();

  input.setAttribute(html_names::kStyleAttr,
                     AtomicString("-webkit-text-security: disc;"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordCSS());

  // Changing type to a non-text control (e.g., checkbox) should not clear the
  // "has ever been" state.
  input.setType(input_type_names::kCheckbox);
  EXPECT_TRUE(input.HasBeenHeuristicCustomPasswordCSS());
}

struct PasswordFieldResetParam {
  const char* new_type;
  const char* temporary_value;
  bool expected_call = true;
};

class HTMLInputElementPasswordFieldResetTest
    : public HTMLInputElementTest,
      public ::testing::WithParamInterface<PasswordFieldResetParam> {
 protected:
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<PasswordResetChromeClient>();
    SetupPageWithClients(chrome_client_);
  }

  PasswordResetChromeClient& chrome_client() { return *chrome_client_; }

 private:
  Persistent<PasswordResetChromeClient> chrome_client_;
};

// Tests that PasswordFieldReset() is (only) called for empty fields. This is
// particularly relevant for field types where setValue("") does not imply
// value().IsEmpty(), such as <input type="range"> (see crbug.com/1265130).
TEST_P(HTMLInputElementPasswordFieldResetTest, PasswordFieldReset) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=password>");
  GetDocument().UpdateStyleAndLayoutTree();

  TestElement().setType(AtomicString(GetParam().new_type));
  GetDocument().UpdateStyleAndLayoutTree();

  TestElement().SetValue(GetParam().temporary_value);
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_CALL(chrome_client(),
              PasswordFieldReset(Truly([this](const HTMLInputElement& e) {
                return e.isSameNode(&TestElement()) && e.Value().empty();
              })))
      .Times(GetParam().expected_call ? 1 : 0);
  TestElement().SetValue("");
  GetDocument().UpdateStyleAndLayoutTree();
}

INSTANTIATE_TEST_SUITE_P(
    HTMLInputElementTest,
    HTMLInputElementPasswordFieldResetTest,
    ::testing::Values(PasswordFieldResetParam{"password", "some_value", true},
                      PasswordFieldResetParam{"text", "some_value", true},
                      PasswordFieldResetParam{"range", "51", false}));

TEST_F(HTMLInputElementTest, TrackPasswordTrackingElementRect) {
  ScopedAIPageContentTrackedElementsPasswordForTest scoped_feature(true);

  viz::TrackedElementFeature tracking_feature =
      viz::TrackedElementFeature::kPasswordTracking;

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=password value='abc'>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);

  EXPECT_TRUE(input->GetTrackedElementSubRect(tracking_feature));

  input->setType(input_type_names::kCheckbox);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(input->GetTrackedElementSubRect(tracking_feature));

  input->setType(input_type_names::kPassword);
  GetDocument().UpdateStyleAndLayoutTree();
  // value is still "abc", so it should track.
  EXPECT_TRUE(input->GetTrackedElementSubRect(tracking_feature));

  input->SetValue(AtomicString(""));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(input->GetTrackedElementSubRect(tracking_feature));

  input->SetValue(AtomicString("def"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(input->GetTrackedElementSubRect(tracking_feature));
}

TEST_F(HTMLInputElementTest,
       TrackPasswordTrackingElementRectJSHeuristicTypeChange) {
  ScopedAIPageContentTrackedElementsPasswordForTest scoped_feature(true);

  viz::TrackedElementFeature tracking_feature =
      viz::TrackedElementFeature::kPasswordTracking;

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=text>");
  auto& input = TestElement();
  GetDocument().UpdateStyleAndLayoutTree();

  // Programmatic value change to a masked pattern triggers tracking.
  input.SetValue("****a");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(input.GetTrackedElementSubRect(tracking_feature));

  // Changing to a non-text field should stop tracking.
  input.setType(input_type_names::kCheckbox);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(input.GetTrackedElementSubRect(tracking_feature));

  // Changing back to text should resume tracking.
  input.setType(input_type_names::kText);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(input.GetTrackedElementSubRect(tracking_feature));
}

TEST_F(HTMLInputElementTest, SpinButtonWheelBlocks) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=number min=0 max=2 value=1>");
  GetDocument().UpdateStyleAndLayoutTree();

  auto& input = TestElement();
  input.Focus();
  auto* shadow_root = input.UserAgentShadowRoot();
  ASSERT_TRUE(shadow_root);
  auto* spin_button = DynamicTo<SpinButtonElement>(
      shadow_root->getElementById(shadow_element_names::kIdSpinButton));
  ASSERT_TRUE(spin_button);

  EventHandlerRegistry& registry =
      GetDocument().GetFrame()->GetEventHandlerRegistry();
  const auto* targets =
      registry.EventHandlerTargets(EventHandlerRegistry::kWheelEventBlocking);
  EXPECT_TRUE(targets->Contains(&input));

  // Wheel event to step up (value goes from 1 to 2).
  WheelEventInit* init = WheelEventInit::Create();
  init->setWheelDeltaY(120);
  WheelEvent* event = WheelEvent::Create(event_type_names::kWheel, init);
  spin_button->ForwardEvent(*event);
  EXPECT_TRUE(event->DefaultHandled());
  EXPECT_EQ("2", input.Value());

  // Wheel event to step up again (value cannot change, already at max 2).
  WheelEvent* event2 = WheelEvent::Create(event_type_names::kWheel, init);
  spin_button->ForwardEvent(*event2);
  EXPECT_TRUE(event2->DefaultHandled());
  EXPECT_EQ("2", input.Value());

  // Changing input type to 'text' should unregister it from
  // kWheelEventBlocking.
  input.setType(input_type_names::kText);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(targets->Contains(&input));

  // Changing input type back to 'number' should re-register it (since it is
  // still focused).
  input.setType(input_type_names::kNumber);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(targets->Contains(&input));

  // Blurring the input should unregister it.
  input.blur();
  EXPECT_FALSE(targets->Contains(&input));

  // Focusing the input should re-register it.
  input.Focus();
  EXPECT_TRUE(targets->Contains(&input));

  // Making the input read-only should unregister it.
  input.SetBooleanAttribute(html_names::kReadonlyAttr, true);
  EXPECT_FALSE(targets->Contains(&input));

  // Making the input read-write should re-register it.
  input.SetBooleanAttribute(html_names::kReadonlyAttr, false);
  EXPECT_TRUE(targets->Contains(&input));

  // Removing the input from the DOM (triggering DetachLayoutTree) should
  // unregister it.
  input.remove();
  EXPECT_FALSE(targets->Contains(&input));
}

}  // namespace blink
