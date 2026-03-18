// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/fake_autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AutofillAIEntityEditMediatorTest : public PlatformTest {
 protected:
  AutofillAIEntityEditMediatorTest() {
    consumer_ = [[FakeAutofillAIEntityEditConsumer alloc] init];
  }

  // Helper method to create a mediator with a given entity instance.
  AutofillAIEntityEditMediator* CreateMediator(
      autofill::EntityInstance instance) {
    AutofillAIEntityEditMediator* mediator =
        [[AutofillAIEntityEditMediator alloc] initWithEntityInstance:instance
                                                   entityDataManager:nil];
    mediator.consumer = consumer_;
    return mediator;
  }

  // Helper method to create a mediator with a given entity instance and
  // run verification.
  void VerifyEntity(autofill::EntityInstance instance,
                    NSUInteger expected_count) {
    CreateMediator(instance);
    EXPECT_NSEQ(consumer_.title,
                base::SysUTF16ToNSString(instance.type().GetNameForI18n()));
    EXPECT_GE(consumer_.editItems.count, expected_count);
  }

  FakeAutofillAIEntityEditConsumer* consumer_;
};

// Tests that the mediator can format a Passport entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensPassport) {
  VerifyEntity(autofill::test::GetPassportEntityInstance(), 3);
}

// Tests that the mediator can format a Driver's License entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensDriversLicense) {
  VerifyEntity(autofill::test::GetDriversLicenseEntityInstance(), 0);
}

// Tests that the mediator can format a Traveler Number entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensTravelerNumber) {
  VerifyEntity(autofill::test::GetKnownTravelerNumberInstance(), 0);
}

// Tests that the mediator can format a Vehicle entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensVehicle) {
  VerifyEntity(autofill::test::GetVehicleEntityInstance(), 0);
}

// Tests that the mediator can format a Redress Number entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensRedressNumber) {
  VerifyEntity(autofill::test::GetRedressNumberEntityInstance(), 0);
}

// Tests that the mediator can format a National Id Card entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensNationalIdCard) {
  VerifyEntity(autofill::test::GetNationalIdCardEntityInstance(), 0);
}

// Tests that the mediator can format a Flight Reservation entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensFlightReservation) {
  VerifyEntity(autofill::test::GetFlightReservationEntityInstance(), 3);
}

// Tests that the mediator can format an Order entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensOrder) {
  VerifyEntity(autofill::test::GetOrderEntityInstance(), 1);
}
