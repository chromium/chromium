// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"

#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

using AutofillAiUiUtilTest = PlatformTest;

// Tests that a local entity returns the standard save-to-device footer notice
// without URLs.
TEST_F(AutofillAiUiUtilTest, LocalEntity_ReturnsSaveToDeviceFooter) {
  autofill::EntityInstance entity = autofill::test::GetPassportEntityInstance(
      {.record_type = autofill::EntityInstance::RecordType::kLocal});

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/false,
                                       u"user@example.com",
                                       /*legal_messages=*/nil);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE),
              content.text);
  EXPECT_EQ(0u, content.urls.count);
  EXPECT_TRUE(content.legal_message_urls.empty());
}

// Tests that a wallet entity with an empty user email falls back to the
// save-to-device footer notice.
TEST_F(AutofillAiUiUtilTest, EmptyEmail_ReturnsSaveToDeviceFooter) {
  autofill::EntityInstance entity = autofill::test::GetPassportEntityInstance(
      {.record_type = autofill::EntityInstance::RecordType::kServerWallet});

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/false, u"",
                                       /*legal_messages=*/nil);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE),
              content.text);
  EXPECT_EQ(0u, content.urls.count);
  EXPECT_TRUE(content.legal_message_urls.empty());
}

// Tests that saving a public pass to Wallet returns the storage notice with
// the manage-your-info URL.
TEST_F(AutofillAiUiUtilTest, WalletPublicPass_Save_ReturnsManageInfoURL) {
  autofill::EntityInstance entity =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/false,
                                       u"user@example.com",
                                       /*legal_messages=*/nil);

  EXPECT_TRUE([content.text containsString:@"BEGIN_LINK"]);
  EXPECT_TRUE([content.text containsString:@"END_LINK"]);
  EXPECT_TRUE([content.text containsString:@"user@example.com"]);
  ASSERT_EQ(1u, content.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), content.urls[0].gurl);
  EXPECT_TRUE(content.legal_message_urls.empty());
}

// Tests that saving a private pass without the refined UX feature enabled
// returns a single manage-your-info URL.
TEST_F(AutofillAiUiUtilTest,
       WalletPrivatePass_Save_WithoutRefinedUx_ReturnsManageInfoURL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillRegionalizedPrivatePassesUxRefine);

  autofill::EntityInstance entity = autofill::test::GetPassportEntityInstance(
      {.record_type = autofill::EntityInstance::RecordType::kServerWallet});

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/false,
                                       u"user@example.com",
                                       /*legal_messages=*/nil);

  EXPECT_TRUE([content.text containsString:@"user@example.com"]);
  ASSERT_EQ(1u, content.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), content.urls[0].gurl);
  EXPECT_TRUE(content.legal_message_urls.empty());
}

// Tests that saving a private pass with the refined UX feature enabled returns
// both the manage-your-info URL and the Privacy Policy URL.
TEST_F(AutofillAiUiUtilTest,
       WalletPrivatePass_Save_WithRefinedUx_ReturnsPrivacyPolicyURL) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillRegionalizedPrivatePassesUxRefine);

  autofill::EntityInstance entity = autofill::test::GetPassportEntityInstance(
      {.record_type = autofill::EntityInstance::RecordType::kServerWallet});

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/false,
                                       u"user@example.com",
                                       /*legal_messages=*/nil);

  EXPECT_TRUE([content.text containsString:@"user@example.com"]);
  ASSERT_EQ(2u, content.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), content.urls[0].gurl);
  EXPECT_EQ(autofill::GetGooglePrivacyPolicyURL(), content.urls[1].gurl);
  EXPECT_TRUE(content.legal_message_urls.empty());
}

// Tests that updating an entity in Wallet returns the update notice with
// the Google Wallet passes URL.
TEST_F(AutofillAiUiUtilTest, WalletEntity_Update_ReturnsPassesURL) {
  autofill::EntityInstance entity =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/true,
                                       u"user@example.com",
                                       /*legal_messages=*/nil);

  EXPECT_TRUE([content.text containsString:@"BEGIN_LINK"]);
  EXPECT_TRUE([content.text containsString:@"END_LINK"]);
  EXPECT_TRUE([content.text containsString:@"user@example.com"]);
  ASSERT_EQ(1u, content.urls.count);
  EXPECT_EQ(autofill::GetGoogleWalletPassesURL(), content.urls[0].gurl);
  EXPECT_TRUE(content.legal_message_urls.empty());
}

// Tests that legal messages are appended to the storage notice and their URLs
// are tracked for metrics.
TEST_F(AutofillAiUiUtilTest, WithLegalMessages_CombinesNoticeAndAppendsURLs) {
  std::vector<autofill::LegalMessageLine> lines = {
      autofill::TestLegalMessageLine(
          "By continuing, you agree to the Terms.",
          {autofill::TestLegalMessageLine::Link(32, 37,
                                                "https://example.com/terms")})};
  NSArray<AutofillLegalMessageLine*>* legal_messages =
      [AutofillLegalMessageLine convertFrom:lines];

  autofill::EntityInstance entity =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/false,
                                       u"user@example.com", legal_messages);

  EXPECT_TRUE([content.text
      containsString:
          @"By continuing, you agree to the BEGIN_LINKTermsEND_LINK."]);
  ASSERT_EQ(2u, content.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), content.urls[0].gurl);
  EXPECT_EQ(GURL("https://example.com/terms"), content.urls[1].gurl);

  ASSERT_EQ(1u, content.legal_message_urls.size());
  EXPECT_EQ(GURL("https://example.com/terms"), content.legal_message_urls[0]);
}

// Tests that an invalid legal message link URL is ignored.
TEST_F(AutofillAiUiUtilTest, WithInvalidLegalMessageURL_OmitsInvalidURL) {
  std::vector<autofill::LegalMessageLine> lines = {
      autofill::TestLegalMessageLine(
          "Notice with invalid link.",
          {autofill::TestLegalMessageLine::Link(12, 24, "not-a-valid-url")})};
  NSArray<AutofillLegalMessageLine*>* legal_messages =
      [AutofillLegalMessageLine convertFrom:lines];

  autofill::EntityInstance entity =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);

  autofill::AutofillAiEntityFooterContent content =
      autofill::GetEntityFooterContent(entity, /*is_update=*/false,
                                       u"user@example.com", legal_messages);

  EXPECT_FALSE([content.text containsString:@"BEGIN_LINKinvalid linkEND_LINK"]);
  EXPECT_TRUE([content.text containsString:@"invalid link"]);
  ASSERT_EQ(1u, content.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), content.urls[0].gurl);
  EXPECT_TRUE(content.legal_message_urls.empty());
}

// Tests that dialog title functions return the expected localized strings.
TEST_F(AutofillAiUiUtilTest, DialogTitles_ReturnExpectedLocalizedStrings) {
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE),
              autofill::GetDialogTitleForSaveEntity(
                  autofill::EntityTypeName::kPassport));
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE),
              autofill::GetDialogTitleForUpdateEntity(
                  autofill::EntityTypeName::kPassport));
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY),
              autofill::GetDialogTitleForAddEntity(
                  autofill::EntityTypeName::kPassport));
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME),
              autofill::GetDialogTitleForViewEntity(
                  autofill::EntityTypeName::kPassport));
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_AUTOFILL_AI_EDIT_PASSPORT_ENTITY),
              autofill::GetDialogTitleForEditEntity(
                  autofill::EntityTypeName::kPassport));
}

// Tests that predefined static URLs are valid HTTPS URLs.
TEST_F(AutofillAiUiUtilTest, StaticURLs_AreValidHttps) {
  EXPECT_TRUE(autofill::GetManageYourInfoURL().is_valid());
  EXPECT_TRUE(autofill::GetManageYourInfoURL().SchemeIs(url::kHttpsScheme));
  EXPECT_TRUE(autofill::GetGooglePrivacyPolicyURL().is_valid());
  EXPECT_TRUE(
      autofill::GetGooglePrivacyPolicyURL().SchemeIs(url::kHttpsScheme));
  EXPECT_TRUE(autofill::GetGoogleWalletPassesURL().is_valid());
  EXPECT_TRUE(autofill::GetGoogleWalletPassesURL().SchemeIs(url::kHttpsScheme));
}

// Tests that `AreCrURLArraysEqual()` compares `CrURL` arrays by their wrapped
// `GURL` values.
TEST_F(AutofillAiUiUtilTest, AreCrURLArraysEqual_ComparesWrappedGURLs) {
  EXPECT_TRUE(autofill::AreCrURLArraysEqual(nil, nil));
  EXPECT_FALSE(autofill::AreCrURLArraysEqual(nil, @[]));
  EXPECT_FALSE(autofill::AreCrURLArraysEqual(@[], nil));
  EXPECT_TRUE(autofill::AreCrURLArraysEqual(@[], @[]));

  CrURL* url_a1 = [[CrURL alloc] initWithGURL:GURL("https://a.com")];
  CrURL* url_a2 = [[CrURL alloc] initWithGURL:GURL("https://a.com")];
  CrURL* url_b = [[CrURL alloc] initWithGURL:GURL("https://b.com")];

  NSArray<CrURL*>* array_a1 = @[ url_a1 ];
  EXPECT_TRUE(autofill::AreCrURLArraysEqual(array_a1, array_a1));

  // Distinct `CrURL` instances with equal `GURL` values.
  EXPECT_TRUE(autofill::AreCrURLArraysEqual(@[ url_a1 ], @[ url_a2 ]));

  // Different counts.
  EXPECT_FALSE(autofill::AreCrURLArraysEqual(@[ url_a1 ], @[ url_a1, url_b ]));

  // Different URLs.
  EXPECT_FALSE(autofill::AreCrURLArraysEqual(@[ url_a1 ], @[ url_b ]));

  // Multiple elements in different orders.
  EXPECT_FALSE(
      autofill::AreCrURLArraysEqual(@[ url_a1, url_b ], @[ url_b, url_a1 ]));
}

}  // namespace
