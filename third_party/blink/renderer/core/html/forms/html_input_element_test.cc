// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/file_input_type.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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
  GetDocument().documentElement()->setInnerHTML("<input id=test>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().empty());

  GetDocument().documentElement()->setInnerHTML(
      "<input id=test list=dl1><datalist id=dl1></datalist>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().empty());
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsContain) {
  GetDocument().documentElement()->setInnerHTML(
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

  GetDocument().documentElement()->setInnerHTML(
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
  GetDocument().documentElement()->setInnerHTML(R"HTML(
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
  GetDocument().documentElement()->setInnerHTML(R"HTML(
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

  GetDocument().documentElement()->setInnerHTML(R"HTML(
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

TEST_F(HTMLInputElementTest, create) {
  auto* input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());

  input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByParser(&GetDocument()));
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  input->ParserSetAttributes(Vector<Attribute, kAttributePrealloc>());
  if (RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled()) {
    EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  } else {
    EXPECT_NE(nullptr, input->UserAgentShadowRoot());
  }
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
      ->setInnerHTML("<input type='range' />");
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
  GetDocument().body()->setInnerHTML(
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
  GetDocument().body()->setInnerHTML(
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
  // InputType::applyStep() should not pass an out-of-range value to
  // setValueAsDecimal, and WTF::msToYear() should not cause a DCHECK failure.
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
  GetDocument().body()->setInnerHTML("<input type='checkbox' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, ChangingInputTypeCausesShadowRootToBeCreated) {
  GetDocument().body()->setInnerHTML("<input type='checkbox' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  EXPECT_NE(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, RepaintAfterClearingFile) {
  GetDocument().body()->setInnerHTML("<input type='file' />");
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
  GetDocument().body()->setInnerHTML("<input/>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);
  EXPECT_FALSE(IsShadowHost(*input));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(IsShadowHost(*input));
}

TEST_F(HTMLInputElementTest, LazilyCreateShadowTreeWithPlaceholder) {
  GetDocument().body()->setInnerHTML("<input placeholder='x'/>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);
  EXPECT_FALSE(IsShadowHost(*input));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(IsShadowHost(*input));
}

TEST_F(HTMLInputElementTest, LazilyCreateShadowTreeWithValue) {
  GetDocument().body()->setInnerHTML("<input value='x'/>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(input);
  EXPECT_FALSE(IsShadowHost(*input));
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
  GetDocument().documentElement()->setInnerHTML(
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

}  // namespace blink
