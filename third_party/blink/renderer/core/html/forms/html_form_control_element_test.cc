// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {
class MockFormValidationMessageClient
    : public GarbageCollected<MockFormValidationMessageClient>,
      public ValidationMessageClient {
 public:
  void ShowValidationMessage(const Element& anchor,
                             const String&,
                             TextDirection,
                             const String&,
                             TextDirection) override {
    anchor_ = anchor;
    ++operation_count_;
  }

  void HideValidationMessage(const Element& anchor) override {
    if (anchor_ == &anchor)
      anchor_ = nullptr;
    ++operation_count_;
  }

  bool IsValidationMessageVisible(const Element& anchor) override {
    return anchor_ == &anchor;
  }

  void DocumentDetached(const Document&) override {}
  void DidChangeFocusTo(const Element*) override {}
  void WillBeDestroyed() override {}
  void Trace(Visitor* visitor) const override {
    visitor->Trace(anchor_);
    ValidationMessageClient::Trace(visitor);
  }

  // The number of calls of ShowValidationMessage() and HideValidationMessage().
  int OperationCount() const { return operation_count_; }

 private:
  Member<const Element> anchor_;
  int operation_count_ = 0;
};
}  // namespace

class HTMLFormControlElementTest : public PageTestBase {
 protected:
  void SetUp() override;
};

void HTMLFormControlElementTest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().SetMimeType("text/html");
}

TEST_F(HTMLFormControlElementTest, customValidationMessageTextDirection) {
  SetHtmlInnerHTML("<body><input pattern='abc' value='def' id=input></body>");

  auto* input = To<HTMLInputElement>(GetElementById("input"));
  input->setCustomValidity(
      String::FromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));
  input->setAttribute(
      html_names::kTitleAttr,
      AtomicString::FromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));

  String message = input->validationMessage().StripWhiteSpace();
  String sub_message = input->ValidationSubMessage().StripWhiteSpace();
  TextDirection message_dir = TextDirection::kRtl;
  TextDirection sub_message_dir = TextDirection::kLtr;

  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kRtl, message_dir);
  EXPECT_EQ(TextDirection::kLtr, sub_message_dir);

  ComputedStyleBuilder rtl_style_builder(input->GetLayoutObject()->StyleRef());
  rtl_style_builder.SetDirection(TextDirection::kRtl);
  input->GetLayoutObject()->SetStyle(rtl_style_builder.TakeStyle());
  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kRtl, message_dir);
  EXPECT_EQ(TextDirection::kLtr, sub_message_dir);

  input->setCustomValidity(String::FromUTF8("Main message."));
  message = input->validationMessage().StripWhiteSpace();
  sub_message = input->ValidationSubMessage().StripWhiteSpace();
  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kLtr, message_dir);
  EXPECT_EQ(TextDirection::kLtr, sub_message_dir);

  input->setCustomValidity(String());
  message = input->validationMessage().StripWhiteSpace();
  sub_message = input->ValidationSubMessage().StripWhiteSpace();
  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kLtr, message_dir);
  EXPECT_EQ(TextDirection::kRtl, sub_message_dir);
}

TEST_F(HTMLFormControlElementTest, UpdateValidationMessageSkippedIfPrinting) {
  SetHtmlInnerHTML("<body><input required id=input></body>");
  ValidationMessageClient* validation_message_client =
      MakeGarbageCollected<MockFormValidationMessageClient>();
  GetPage().SetValidationMessageClientForTesting(validation_message_client);
  Page::OrdinaryPages().insert(&GetPage());

  auto* input = To<HTMLInputElement>(GetElementById("input"));
  ScopedPagePauser pauser;  // print() pauses the page.
  input->reportValidity();
  EXPECT_FALSE(validation_message_client->IsValidationMessageVisible(*input));
}

TEST_F(HTMLFormControlElementTest, DoNotUpdateLayoutDuringDOMMutation) {
  // The real ValidationMessageClient has UpdateStyleAndLayout*() in
  // ShowValidationMessage(). So calling it during DOM mutation is
  // dangerous. This test ensures ShowValidationMessage() is NOT called in
  // appendChild(). crbug.com/756408
  GetDocument().documentElement()->setInnerHTML("<select></select>");
  auto* const select =
      To<HTMLFormControlElement>(GetDocument().QuerySelector("select"));
  auto* const optgroup =
      GetDocument().CreateRawElement(html_names::kOptgroupTag);
  auto* validation_client =
      MakeGarbageCollected<MockFormValidationMessageClient>();
  GetDocument().GetPage()->SetValidationMessageClientForTesting(
      validation_client);

  select->setCustomValidity("foobar");
  select->reportValidity();
  int start_operation_count = validation_client->OperationCount();
  select->appendChild(optgroup);
  EXPECT_EQ(start_operation_count, validation_client->OperationCount())
      << "DOM mutation should not handle validation message UI in it.";
}

TEST_F(HTMLFormControlElementTest, UniqueRendererFormControlId) {
  SetHtmlInnerHTML("<body><input id=input1><input id=input2></body>");
  auto* form_control1 = To<HTMLFormControlElement>(GetElementById("input1"));
  uint64_t first_id = form_control1->UniqueRendererFormControlId();
  auto* form_control2 = To<HTMLFormControlElement>(GetElementById("input2"));
  EXPECT_EQ(first_id + 1, form_control2->UniqueRendererFormControlId());
  SetHtmlInnerHTML("<body><select id=select1></body>");
  auto* form_control3 = To<HTMLFormControlElement>(GetElementById("select1"));
  EXPECT_EQ(first_id + 2, form_control3->UniqueRendererFormControlId());
}

}  // namespace blink
