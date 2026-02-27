// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_infobar_delegate_ios.h"

#import "base/functional/callback_helpers.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

namespace autofill {

const char16_t kTestUserEmail[] = u"test@example.com";

class AutofillAiSaveEntityInfoBarDelegateIOSTest
    : public PlatformTest,
      public ::testing::WithParamInterface<EntityTypeName> {
 public:
  AutofillAiSaveEntityInfoBarDelegateIOSTest() = default;

 protected:
  EntityInstance GetEntityInstanceForType(EntityTypeName type) {
    switch (type) {
      case EntityTypeName::kPassport:
        return test::GetPassportEntityInstance();
      case EntityTypeName::kDriversLicense:
        return test::GetDriversLicenseEntityInstance();
      case EntityTypeName::kVehicle:
        return test::GetVehicleEntityInstance();
      case EntityTypeName::kNationalIdCard:
        return test::GetNationalIdCardEntityInstance();
      case EntityTypeName::kKnownTravelerNumber:
        return test::GetKnownTravelerNumberInstance();
      case EntityTypeName::kRedressNumber:
        return test::GetRedressNumberEntityInstance();
      case EntityTypeName::kOrder:
        return test::GetOrderEntityInstance();
      case EntityTypeName::kFlightReservation:
        return test::GetFlightReservationEntityInstance();
    }
    NOTREACHED();
  }

  base::test::TaskEnvironment task_environment_;
};

// Tests that when one entity is provided, the infobar prompts to save it to
// the device.
TEST_P(AutofillAiSaveEntityInfoBarDelegateIOSTest, GetMessageTextSave) {
  EntityInstance entity = GetEntityInstanceForType(GetParam());
  SaveEntityParams params(entity, std::nullopt, kTestUserEmail,
                          base::DoNothing());
  AutofillAiSaveEntityInfoBarDelegateIOS delegate(std::move(params),
                                                  base::DoNothing());

  std::u16string expected_text =
      l10n_util::GetStringUTF16(IDS_IOS_INFOBAR_MESSAGE_SAVE_TO_DEVICE);

  EXPECT_EQ(delegate.GetMessageText(), expected_text);
}

// Tests that when old entity and new entity are provided, the infobar prompts
// to save the new entity to the device.
TEST_P(AutofillAiSaveEntityInfoBarDelegateIOSTest, GetMessageTextUpdate) {
  EntityInstance entity = GetEntityInstanceForType(GetParam());
  SaveEntityParams params(entity, entity, kTestUserEmail, base::DoNothing());
  AutofillAiSaveEntityInfoBarDelegateIOS delegate(std::move(params),
                                                  base::DoNothing());

  EXPECT_EQ(delegate.GetMessageText(),
            l10n_util::GetStringUTF16(IDS_IOS_INFOBAR_MESSAGE_SAVE_TO_DEVICE));
}

// Tests that the button labels are correct for the save case.
TEST_P(AutofillAiSaveEntityInfoBarDelegateIOSTest, GetButtonLabelSave) {
  EntityInstance entity = GetEntityInstanceForType(GetParam());
  SaveEntityParams params(entity, std::nullopt, kTestUserEmail,
                          base::DoNothing());
  AutofillAiSaveEntityInfoBarDelegateIOS delegate(std::move(params),
                                                  base::DoNothing());

  EXPECT_EQ(delegate.GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
  EXPECT_EQ(
      delegate.GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
      l10n_util::GetStringUTF16(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_NO_THANKS));
}

// Tests that the button labels are correct for the update case.
TEST_P(AutofillAiSaveEntityInfoBarDelegateIOSTest, GetButtonLabelUpdate) {
  EntityInstance entity = GetEntityInstanceForType(GetParam());
  SaveEntityParams params(entity, entity, kTestUserEmail, base::DoNothing());
  AutofillAiSaveEntityInfoBarDelegateIOS delegate(std::move(params),
                                                  base::DoNothing());

  EXPECT_EQ(delegate.GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
  EXPECT_EQ(
      delegate.GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
      l10n_util::GetStringUTF16(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_NO_THANKS));
}

// Tests that the icon is correct for the save case. Since update case uses
// the same icon, we don't need to test it separately.
TEST_P(AutofillAiSaveEntityInfoBarDelegateIOSTest, GetIcon) {
  EntityInstance entity = GetEntityInstanceForType(GetParam());
  SaveEntityParams params(entity, std::nullopt, kTestUserEmail,
                          base::DoNothing());
  AutofillAiSaveEntityInfoBarDelegateIOS delegate(std::move(params),
                                                  base::DoNothing());

  // Since we use SF Symbols, we just check that it's not empty and not the
  // default if it's a known type.
  EXPECT_FALSE(delegate.GetIcon().IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(AllEntityTypes,
                         AutofillAiSaveEntityInfoBarDelegateIOSTest,
                         ::testing::Values(EntityTypeName::kPassport,
                                           EntityTypeName::kOrder,
                                           EntityTypeName::kDriversLicense,
                                           EntityTypeName::kVehicle,
                                           EntityTypeName::kNationalIdCard,
                                           EntityTypeName::kKnownTravelerNumber,
                                           EntityTypeName::kRedressNumber,
                                           EntityTypeName::kFlightReservation));

}  // namespace autofill
