// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/fake_at_memory_fill_handler.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::MemoryDataType;
using autofill::Suggestion;
using autofill::SuggestionType;

namespace {

NSString* const kPassportTitle = @"Passport";
NSString* const kPassportNumberLabel = @"Passport Number";
NSString* const kPassportNumberValue = @"AA123456";

Suggestion CreateTestSuggestionWithChildren() {
  Suggestion parent(base::SysNSStringToUTF16(kPassportTitle),
                    SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload parent_payload(
      base::SysNSStringToUTF16(kPassportNumberValue),
      MemoryDataType::kPassportNumber);
  parent_payload.type_name = base::SysNSStringToUTF16(kPassportTitle);
  parent.payload = std::move(parent_payload);

  Suggestion child(base::SysNSStringToUTF16(kPassportNumberValue),
                   SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload child_payload(
      base::SysNSStringToUTF16(kPassportNumberValue),
      MemoryDataType::kPassportNumber);
  child_payload.type_name = base::SysNSStringToUTF16(kPassportNumberLabel);
  child.payload = std::move(child_payload);

  parent.children.push_back(std::move(child));
  return parent;
}

}  // namespace

class AtMemoryGranularFillMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_consumer_ = OCMProtocolMock(@protocol(AtMemoryGranularFillConsumer));
    mock_at_memory_handler_ = OCMProtocolMock(@protocol(AtMemoryCommands));
  }

  void TearDown() override {
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  void CreateMediator(Suggestion suggestion) {
    mediator_ = [[AtMemoryGranularFillMediator alloc]
        initWithSuggestion:std::move(suggestion)];
    mediator_.consumer = mock_consumer_;
    mediator_.atMemoryHandler = mock_at_memory_handler_;
  }

  id mock_consumer_;
  id mock_at_memory_handler_;
  AtMemoryGranularFillMediator* mediator_;
};

// Tests that setting consumer pushes title and granular fill items.
TEST_F(AtMemoryGranularFillMediatorTest, TestConsumerGetsItems) {
  OCMExpect([mock_consumer_ setTitle:kPassportTitle]);
  OCMExpect([mock_consumer_
      setGranularFillItems:[OCMArg
                               checkWithBlock:^BOOL(
                                   NSArray<AtMemoryGranularFillItem*>* items) {
                                 return items.count == 1;
                               }]]);

  CreateMediator(CreateTestSuggestionWithChildren());

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that selecting a granular fill item delegates to the fill handler.
TEST_F(AtMemoryGranularFillMediatorTest,
       SelectGranularFillItemDelegatesToFillHandler) {
  CreateMediator(CreateTestSuggestionWithChildren());

  FakeAtMemoryFillHandler* fake_fill_handler =
      [[FakeAtMemoryFillHandler alloc] init];
  mediator_.fillHandler = fake_fill_handler;

  AtMemoryGranularFillItem* item = [[AtMemoryGranularFillItem alloc]
      initWithAttributeName:kPassportNumberLabel
             attributeValue:kPassportNumberValue
                      index:0];

  [mediator_ didSelectGranularFillItem:item];

  EXPECT_TRUE(fake_fill_handler.fillWithSuggestionCalled);
}

// Tests that selecting an invalid granular fill item dismisses the UI.
TEST_F(AtMemoryGranularFillMediatorTest,
       SelectGranularFillItemInvalidIndexDismisses) {
  CreateMediator(CreateTestSuggestionWithChildren());

  AtMemoryGranularFillItem* item = [[AtMemoryGranularFillItem alloc]
      initWithAttributeName:kPassportNumberLabel
             attributeValue:kPassportNumberValue
                      index:-1];

  OCMExpect([mock_at_memory_handler_ dismissAtMemory]);

  [mediator_ didSelectGranularFillItem:item];

  EXPECT_OCMOCK_VERIFY(mock_at_memory_handler_);
}
