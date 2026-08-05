// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// ISO alpha-2 country codes.
NSString* const kSouthKoreaCountryCode = @"kr";
NSString* const kUSCountryCode = @"us";

// Helper function to check for a link in an attributed string.
BOOL HasLinkWithAction(NSAttributedString* attributedText, NSString* action) {
  __block BOOL found_link = NO;
  [attributedText
      enumerateAttribute:NSLinkAttributeName
                 inRange:NSMakeRange(0, attributedText.length)
                 options:0
              usingBlock:^(id value, NSRange range, BOOL* stop) {
                if ([value isKindOfClass:[NSString class]] &&
                    [static_cast<NSString*>(value) isEqualToString:action]) {
                  found_link = YES;
                  *stop = YES;
                }
              }];
  return found_link;
}

}  // namespace

class GeminiConsentConfigurationTest : public PlatformTest {
 protected:
  GeminiConsentConfiguration* BuildStandardConfiguration(BOOL is_managed,
                                                         BOOL use_strict,
                                                         NSString* country) {
    return [GeminiConsentConfiguration
        configurationForManaged:is_managed
                         strict:use_strict
                           type:GeminiFirstRunType::kNewUser
                        country:country];
  }

  GeminiConsentConfiguration* BuildLiveConfiguration(BOOL is_managed) {
    return [GeminiConsentConfiguration
        configurationForManaged:is_managed
                         strict:NO
                           type:GeminiFirstRunType::kLive
                        country:kUSCountryCode];
  }
};

// Tests that standard configuration rows count is exactly 2.
TEST_F(GeminiConsentConfigurationTest, StandardRowCount) {
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, NO, kUSCountryCode);
  ASSERT_NE(nil, config);
  EXPECT_EQ(2U, config.rows.count);
  EXPECT_FALSE(config.collapsible);
  EXPECT_FALSE(config.useStrict);
  EXPECT_EQ(nil, config.header);
}

// Tests properties for non-managed accounts.
TEST_F(GeminiConsentConfigurationTest, StandardNonManagedAccountRows) {
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, NO, kUSCountryCode);

  GeminiConsentRow* row1 = config.rows[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FIRST_BOX_TITLE),
              row1.title);
  EXPECT_TRUE([row1.body.string
      containsString:l10n_util::GetNSString(
                         IDS_IOS_BWG_CONSENT_NON_MANAGED_FIRST_BOX_BODY)]);

  GeminiConsentRow* row2 = config.rows[1];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_TITLE),
      row2.title);
  StringWithTags parsedText2 = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY));
  EXPECT_NSEQ(parsedText2.string, row2.body.string);
  EXPECT_TRUE(HasLinkWithAction(row2.body,
                                kGeminiSecondBoxLink1ActionNonManagedAccount));
  EXPECT_TRUE(HasLinkWithAction(row2.body,
                                kGeminiSecondBoxLink2ActionNonManagedAccount));
}

// Tests properties for managed accounts.
TEST_F(GeminiConsentConfigurationTest, StandardManagedAccountRows) {
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(YES, NO, kUSCountryCode);

  GeminiConsentRow* row1 = config.rows[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FIRST_BOX_TITLE),
              row1.title);
  EXPECT_TRUE([row1.body.string
      containsString:l10n_util::GetNSString(
                         IDS_IOS_BWG_CONSENT_MANAGED_FIRST_BOX_BODY)]);

  GeminiConsentRow* row2 = config.rows[1];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_TITLE),
      row2.title);
  StringWithTags parsedText2 = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY));
  EXPECT_NSEQ(parsedText2.string, row2.body.string);
  EXPECT_TRUE(
      HasLinkWithAction(row2.body, kGeminiSecondBoxLinkActionManagedAccount));
}

// Tests footnote links and country additions.
TEST_F(GeminiConsentConfigurationTest, FootnoteForUSAndNonUS) {
  // US Footnote
  GeminiConsentConfiguration* us_config =
      BuildStandardConfiguration(NO, NO, kUSCountryCode);
  EXPECT_TRUE(
      HasLinkWithAction(us_config.footnote, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(us_config.footnote, kGeminiSecondFootnoteLinkAction));
  EXPECT_TRUE([us_config.footnote.string
      containsString:l10n_util::GetNSString(
                         IDS_IOS_GEMINI_CONSENT_FOOTNOTE_US_ONLY_ADDITION)]);

  // Korea Footnote
  GeminiConsentConfiguration* kr_config =
      BuildStandardConfiguration(NO, NO, kSouthKoreaCountryCode);
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiKoreanTermsLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiSecondFootnoteLinkAction));
  EXPECT_FALSE([kr_config.footnote.string
      containsString:l10n_util::GetNSString(
                         IDS_IOS_GEMINI_CONSENT_FOOTNOTE_US_ONLY_ADDITION)]);
}

// Tests footnote strict watch link and regional strict strings.
TEST_F(GeminiConsentConfigurationTest, StrictConsentFootnote) {
  // US / ROW Strict Footnote
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, YES, kUSCountryCode);
  EXPECT_TRUE(HasLinkWithAction(config.footnote, kGeminiWatchLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(config.footnote, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(config.footnote, kGeminiSecondFootnoteLinkAction));
  EXPECT_TRUE(HasLinkWithAction(config.footnote, kGeminiChoicesLinkAction));
  StringWithTags parsedStrictUS = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_FOOTNOTE_TEXT_STRICT));
  EXPECT_TRUE([config.footnote.string containsString:parsedStrictUS.string]);

  // Korea Strict Footnote
  GeminiConsentConfiguration* kr_config =
      BuildStandardConfiguration(NO, YES, kSouthKoreaCountryCode);
  EXPECT_TRUE(HasLinkWithAction(kr_config.footnote, kGeminiWatchLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiKoreanTermsLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiSecondFootnoteLinkAction));
  EXPECT_TRUE(HasLinkWithAction(kr_config.footnote, kGeminiActivityLinkAction));
  EXPECT_TRUE(HasLinkWithAction(kr_config.footnote, kGeminiChoicesLinkAction));
  StringWithTags parsedStrictKR = ParseStringWithLinks(l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA_STRICT));
  EXPECT_TRUE([kr_config.footnote.string containsString:parsedStrictKR.string]);
}

// Tests that Live configuration has exactly 3 rows and a custom header.
TEST_F(GeminiConsentConfigurationTest, LiveConfigurationStructure) {
  GeminiConsentConfiguration* config = BuildLiveConfiguration(NO);
  ASSERT_NE(nil, config);
  EXPECT_EQ(3U, config.rows.count);
  EXPECT_FALSE(config.collapsible);
  EXPECT_FALSE(config.useStrict);

  // Header
  ASSERT_NE(nil, config.header);
  EXPECT_NE(nil, config.header.icon);
  EXPECT_GT(config.header.title.length, 0U);
}

// Tests properties of Live rows and links.
TEST_F(GeminiConsentConfigurationTest, LiveRowPropertiesAndLinks) {
  GeminiConsentConfiguration* config = BuildLiveConfiguration(NO);

  // Row 1
  GeminiConsentRow* row1 = config.rows[0];
  EXPECT_EQ(nil, row1.title);
  EXPECT_GT(row1.body.length, 0U);

  // Row 2
  GeminiConsentRow* row2 = config.rows[1];
  EXPECT_EQ(nil, row2.title);
  StringWithTags parsedText2 = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_CONSENT_SECOND_BOX_BODY));
  EXPECT_NSEQ(parsedText2.string, row2.body.string);
  EXPECT_TRUE(HasLinkWithAction(row2.body, kGeminiLivePrivacyNoticeLinkAction));
  EXPECT_TRUE(HasLinkWithAction(row2.body, kGeminiLiveLearnMoreLinkAction));

  // Row 3
  GeminiConsentRow* row3 = config.rows[2];
  EXPECT_EQ(nil, row3.title);
  StringWithTags parsedText3 = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_CONSENT_THIRD_BOX_BODY));
  EXPECT_NSEQ(parsedText3.string, row3.body.string);
  EXPECT_TRUE(HasLinkWithAction(row3.body, kGeminiLivePrivacyPolicyLinkAction));
}

// Tests properties of Live rows and links for managed accounts.
TEST_F(GeminiConsentConfigurationTest, LiveRowPropertiesAndLinksForManaged) {
  GeminiConsentConfiguration* config = BuildLiveConfiguration(YES);
  GeminiConsentRow* row = config.rows[1];
  EXPECT_EQ(nil, row.title);
  StringWithTags parsedText = ParseStringWithLinks(l10n_util::GetNSString(
      IDS_IOS_GEMINI_LIVE_CONSENT_MANAGED_SECOND_BOX_BODY));
  EXPECT_NSEQ(parsedText.string, row.body.string);
  EXPECT_TRUE(
      HasLinkWithAction(row.body, kGeminiLivePrivacyHubManagedLinkAction));
}

// Tests properties of the updated consent normal layout.
TEST_F(GeminiConsentConfigurationTest, UpdatedConsentNormalLayout) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGeminiUpdatedConsent);

  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, NO, kUSCountryCode);
  ASSERT_NE(nil, config);
  EXPECT_EQ(2U, config.rows.count);
  EXPECT_FALSE(config.collapsible);
  EXPECT_FALSE(config.useStrict);

  // Row 1: Share Tab
  GeminiConsentRow* row1 = config.rows[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_TITLE),
              row1.title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_BODY),
              row1.body.string);

  // Row 2: Normal Governance
  GeminiConsentRow* row2 = config.rows[1];
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_NON_MANAGED_TITLE),
              row2.title);
  StringWithTags parsedText2 = ParseStringWithLinks(l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_NON_MANAGED_BODY));
  EXPECT_NSEQ(parsedText2.string, row2.body.string);
  EXPECT_TRUE(HasLinkWithAction(row2.body, kGeminiActivityLinkAction));
  EXPECT_TRUE(HasLinkWithAction(row2.body, kGeminiChoicesLinkAction));
}

// Tests properties of the updated consent managed layout.
TEST_F(GeminiConsentConfigurationTest, UpdatedConsentManagedLayout) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGeminiUpdatedConsent);

  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(YES, NO, kUSCountryCode);
  ASSERT_NE(nil, config);
  EXPECT_EQ(2U, config.rows.count);
  EXPECT_FALSE(config.collapsible);
  EXPECT_FALSE(config.useStrict);

  // Row 1: Share Tab
  GeminiConsentRow* row1 = config.rows[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_TITLE),
              row1.title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_BODY),
              row1.body.string);

  // Row 2: Managed Governance
  GeminiConsentRow* row2 = config.rows[1];
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_MANAGED_TITLE),
              row2.title);
  StringWithTags parsedText2 = ParseStringWithLinks(l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_MANAGED_BODY));
  EXPECT_NSEQ(parsedText2.string, row2.body.string);
  EXPECT_TRUE(
      HasLinkWithAction(row2.body, kGeminiDataGovernanceManagedLinkAction));
}

// Tests properties of the updated consent strict layout.
TEST_F(GeminiConsentConfigurationTest, UpdatedConsentStrictLayout) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGeminiUpdatedConsent);

  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, YES, kUSCountryCode);
  ASSERT_NE(nil, config);
  EXPECT_EQ(3U, config.rows.count);
  EXPECT_TRUE(config.collapsible);
  EXPECT_TRUE(config.useStrict);

  // Row 1: Share Tab (Strict)
  GeminiConsentRow* row1 = config.rows[0];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_TITLE_STRICT),
      row1.title);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_BODY_STRICT),
      row1.body.string);

  // Row 2: Connected Services
  GeminiConsentRow* row2 = config.rows[1];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_CONNECTED_SERVICES_TITLE),
      row2.title);
  StringWithTags parsedText2 = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_CONNECTED_SERVICES_BODY));
  EXPECT_NSEQ(parsedText2.string, row2.body.string);
  EXPECT_TRUE(HasLinkWithAction(row2.body, kGeminiConnectedServicesLinkAction));

  // Row 3: Strict Governance
  GeminiConsentRow* row3 = config.rows[2];
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_TITLE),
              row3.title);
  EXPECT_TRUE([row3.body.string
      containsString:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_SUBTITLE_1)]);
  EXPECT_TRUE([row3.body.string
      containsString:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_BODY_1)]);
  EXPECT_TRUE([row3.body.string
      containsString:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_SUBTITLE_2)]);
  StringWithTags parsedStrictBody2 =
      ParseStringWithLinks(l10n_util::GetNSString(
          IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_BODY_2));
  EXPECT_TRUE([row3.body.string containsString:parsedStrictBody2.string]);
  EXPECT_TRUE([row3.body.string
      containsString:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_BODY_3)]);
  EXPECT_TRUE(HasLinkWithAction(row3.body, kGeminiActivityLinkAction));
}
