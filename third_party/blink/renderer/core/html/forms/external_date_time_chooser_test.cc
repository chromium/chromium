// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/external_date_time_chooser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExternalDateTimeChooserTest : public PageTestBase {};

class TestDateTimeChooserClient final
    : public GarbageCollected<TestDateTimeChooserClient>,
      public DateTimeChooserClient {
 public:
  explicit TestDateTimeChooserClient(Element* element) : element_(element) {}
  ~TestDateTimeChooserClient() override {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(element_);
    visitor->Trace(date_time_chooser_);
    DateTimeChooserClient::Trace(visitor);
  }

  void SetDateTimeChooser(DateTimeChooser* date_time_chooser) {
    date_time_chooser_ = date_time_chooser;
  }

 private:
  // DateTimeChooserClient functions:
  Element& OwnerElement() const override { return *element_; }
  void DidChooseValue(const String&) override {}
  void DidChooseValue(double value) override {
    if (date_time_chooser_)
      date_time_chooser_->EndChooser();
  }
  void DidEndChooser() override {}

  Member<Element> element_;
  Member<DateTimeChooser> date_time_chooser_;
};

// This is a regression test for crbug.com/974646. EndChooser can cause a crash
// when it's called twice because |client_| was already nullptr.
TEST_F(ExternalDateTimeChooserTest, EndChooserShouldNotCrash) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* element = document->CreateRawElement(html_names::kInputTag);
  auto* client = MakeGarbageCollected<TestDateTimeChooserClient>(element);
  auto* external_date_time_chooser =
      MakeGarbageCollected<ExternalDateTimeChooser>(client);
  client->SetDateTimeChooser(external_date_time_chooser);
  external_date_time_chooser->ResponseHandler(true, 0);
}

// This is a regression test for crbug.com/1022302. When the label and the value
// are the same in an option element,
// HTMLInputElement::SetupDateTimeChooserParameters had set a null value. This
// caused a crash because Mojo message pipe couldn't get a null pointer at the
// receiving side.
TEST_F(ExternalDateTimeChooserTest,
       OpenDateTimeChooserShouldNotCrashWhenLabelAndValueIsTheSame) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
      <input id=test type="date" list="src" />
        <datalist id="src">
          <option value='2019-12-31'>Hint</option>
          <option value='2019-12-30'/>
          <option>2019-12-29</option> // This has the same value in label and
                                      // value attribute.
        </datalist>
      )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("test")));
  ASSERT_TRUE(input);

  DateTimeChooserParameters params;
  bool success = input->SetupDateTimeChooserParameters(params);
  EXPECT_TRUE(success);

  auto* client = MakeGarbageCollected<TestDateTimeChooserClient>(
      GetDocument().documentElement());
  auto* external_date_time_chooser =
      MakeGarbageCollected<ExternalDateTimeChooser>(client);
  client->SetDateTimeChooser(external_date_time_chooser);
  external_date_time_chooser->OpenDateTimeChooser(GetDocument().GetFrame(),
                                                  params);
  // Crash should not happen after calling OpenDateTimeChooser().
}

TEST_F(ExternalDateTimeChooserTest, IsPickerVisible) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input id=test type=date>");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("test")));
  ASSERT_TRUE(input);

  DateTimeChooserParameters params;
  bool success = input->SetupDateTimeChooserParameters(params);
  ASSERT_TRUE(success);

  auto* client = MakeGarbageCollected<TestDateTimeChooserClient>(
      GetDocument().documentElement());
  auto* external_date_time_chooser =
      MakeGarbageCollected<ExternalDateTimeChooser>(client);
  client->SetDateTimeChooser(external_date_time_chooser);
  EXPECT_FALSE(external_date_time_chooser->IsPickerVisible());

  external_date_time_chooser->OpenDateTimeChooser(GetDocument().GetFrame(),
                                                  params);
  EXPECT_TRUE(external_date_time_chooser->IsPickerVisible());

  external_date_time_chooser->EndChooser();
  EXPECT_FALSE(external_date_time_chooser->IsPickerVisible());

  external_date_time_chooser->OpenDateTimeChooser(GetDocument().GetFrame(),
                                                  params);
  EXPECT_TRUE(external_date_time_chooser->IsPickerVisible());
}

TEST_F(ExternalDateTimeChooserTest, FinePointerFocusability) {
  GetDocument().GetFrame()->GetSettings()->SetAvailablePointerTypes(
      static_cast<int>(mojom::blink::PointerType::kPointerFineType));

  SetBodyContent("<input id=test type=date>");

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("test")));
  ASSERT_TRUE(input);

  ShadowRoot* shadow = input->UserAgentShadowRoot();
  ASSERT_TRUE(shadow);

  Element* edit = shadow->getElementById(shadow_element_names::kIdDateTimeEdit);
  ASSERT_TRUE(edit);
  Element* wrapper = ElementTraversal::FirstChild(*edit);
  ASSERT_TRUE(wrapper);
  Element* subfield = ElementTraversal::FirstChild(*wrapper);
  ASSERT_TRUE(subfield);

  Element* picker_indicator =
      shadow->getElementById(shadow_element_names::kIdPickerIndicator);
  ASSERT_TRUE(picker_indicator);

  EXPECT_TRUE(subfield->IsFocusable());
  EXPECT_TRUE(picker_indicator->IsFocusable());
}

TEST_F(ExternalDateTimeChooserTest, NonFinePointerFocusability) {
  GetDocument().GetFrame()->GetSettings()->SetAvailablePointerTypes(
      static_cast<int>(mojom::blink::PointerType::kPointerCoarseType));

  SetBodyContent("<input id=test type=date>");

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("test")));
  ASSERT_TRUE(input);

  ShadowRoot* shadow = input->UserAgentShadowRoot();
  ASSERT_TRUE(shadow);

  Element* edit = shadow->getElementById(shadow_element_names::kIdDateTimeEdit);
  ASSERT_TRUE(edit);
  Element* wrapper = ElementTraversal::FirstChild(*edit);
  ASSERT_TRUE(wrapper);
  Element* subfield = ElementTraversal::FirstChild(*wrapper);
  ASSERT_TRUE(subfield);

  Element* picker_indicator =
      shadow->getElementById(shadow_element_names::kIdPickerIndicator);
  ASSERT_TRUE(picker_indicator);

  EXPECT_FALSE(subfield->IsFocusable());
  EXPECT_FALSE(picker_indicator->IsFocusable());
}

}  // namespace blink
