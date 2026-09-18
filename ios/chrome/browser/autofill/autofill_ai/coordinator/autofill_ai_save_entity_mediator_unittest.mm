// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

autofill::EntityInstance CreateTestEntity() {
  return autofill::test::GetPassportEntityInstance();
}

}  // namespace

@interface FakeAutofillAISaveEntityConsumer
    : NSObject <AutofillAISaveEntityConsumer>
@property(nonatomic, strong) NSArray<AutofillLegalMessageLine*>* legalMessages;
@property(nonatomic, assign) BOOL setNewEntityCalled;
@end

@implementation FakeAutofillAISaveEntityConsumer

- (void)setNewEntity:(autofill::EntityInstance)newEntity
            oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
            userEmail:(const std::u16string&)userEmail
    saveIsSynchronous:(BOOL)saveIsSynchronous {
  self.setNewEntityCalled = YES;
}

- (void)showLoadingState {
}

- (void)showConfirmationState {
}

@end

class AutofillAISaveEntityMediatorTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// Tests that legal messages are pushed to the consumer when present in params.
TEST_F(AutofillAISaveEntityMediatorTest, PushesLegalMessagesWhenPresent) {
  autofill::LegalMessageLines legal_message_lines = {
      autofill::TestLegalMessageLine("Test legal message")};

  autofill::SaveEntityParams params(
      CreateTestEntity(), /*old_entity=*/std::nullopt,
      /*user_email=*/u"test@example.com",
      /*save_is_synchronous=*/true,
      /*callback=*/base::DoNothing(),
      /*public_passes_notice=*/std::move(legal_message_lines));

  AutofillAISaveEntityMediator* mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                         entityDataManager:nullptr];

  FakeAutofillAISaveEntityConsumer* consumer =
      [[FakeAutofillAISaveEntityConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_TRUE(consumer.setNewEntityCalled);
  ASSERT_NE(nil, consumer.legalMessages);
  ASSERT_EQ(1u, consumer.legalMessages.count);
  EXPECT_NSEQ(@"Test legal message", consumer.legalMessages[0].messageText);

  [mediator disconnect];
}

// Tests that no legal messages are pushed to the consumer when params has empty
// legal message lines.
TEST_F(AutofillAISaveEntityMediatorTest, DoesNotPushLegalMessagesWhenEmpty) {
  autofill::SaveEntityParams params(CreateTestEntity(),
                                    /*old_entity=*/std::nullopt,
                                    /*user_email=*/u"test@example.com",
                                    /*save_is_synchronous=*/true,
                                    /*callback=*/base::DoNothing(),
                                    /*public_passes_notice=*/{});

  AutofillAISaveEntityMediator* mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                         entityDataManager:nullptr];

  FakeAutofillAISaveEntityConsumer* consumer =
      [[FakeAutofillAISaveEntityConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_TRUE(consumer.setNewEntityCalled);
  EXPECT_EQ(nil, consumer.legalMessages);

  [mediator disconnect];
}
