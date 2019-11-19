// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/external_date_time_chooser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ExternalDateTimeChooserTest : public testing::Test {};

class TestDateTimeChooserClient final
    : public GarbageCollected<TestDateTimeChooserClient>,
      public DateTimeChooserClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestDateTimeChooserClient);

 public:
  explicit TestDateTimeChooserClient(Element* element) : element_(element) {}
  ~TestDateTimeChooserClient() override {}

  void Trace(Visitor* visitor) override {
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
  ScopedInputMultipleFieldsUIForTest input_multiple_fields_ui(false);
  auto* document = MakeGarbageCollected<Document>();
  auto* element = document->CreateRawElement(html_names::kInputTag);
  auto* client = MakeGarbageCollected<TestDateTimeChooserClient>(element);
  auto* external_date_time_chooser = ExternalDateTimeChooser::Create(client);
  client->SetDateTimeChooser(external_date_time_chooser);
  external_date_time_chooser->ResponseHandler(true, 0);
}

}  // namespace blink
