// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/external_date_time_chooser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class ExternalDateTimeChooserTest : public testing::Test {
 protected:
  void SetUp() final {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

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
  ScopedInputMultipleFieldsUIForTest input_multiple_fields_ui(false);
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
  ScopedInputMultipleFieldsUIForTest input_multiple_fields_ui(false);
  GetDocument().documentElement()->setInnerHTML(R"HTML(
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

}  // namespace blink
