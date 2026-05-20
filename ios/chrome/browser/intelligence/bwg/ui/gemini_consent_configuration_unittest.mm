// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

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
                           type:GeminiFREType::kNewUser
                        country:country];
  }
};

// Tests that standard configuration rows count is exactly 2.
TEST_F(GeminiConsentConfigurationTest, StandardRowCount) {
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, NO, @"us");
  ASSERT_NE(nil, config);
  EXPECT_EQ(2U, config.rows.count);
  EXPECT_FALSE(config.collapsible);
  EXPECT_EQ(nil, config.header);
}

// Tests properties for non-managed accounts.
TEST_F(GeminiConsentConfigurationTest, StandardNonManagedAccountRows) {
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, NO, @"us");

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
  EXPECT_TRUE([row2.body.string
      containsString:
          l10n_util::GetNSString(
              IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY_LINK_1)]);
  EXPECT_TRUE(HasLinkWithAction(row2.body,
                                kGeminiSecondBoxLink1ActionNonManagedAccount));
  EXPECT_TRUE(HasLinkWithAction(row2.body,
                                kGeminiSecondBoxLink2ActionNonManagedAccount));
}

// Tests properties for managed accounts.
TEST_F(GeminiConsentConfigurationTest, StandardManagedAccountRows) {
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(YES, NO, @"us");

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
  EXPECT_TRUE([row2.body.string
      containsString:l10n_util::GetNSString(
                         IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY_LINK)]);
  EXPECT_TRUE(
      HasLinkWithAction(row2.body, kGeminiSecondBoxLinkActionManagedAccount));
}

// Tests footnote links and country additions.
TEST_F(GeminiConsentConfigurationTest, FootnoteForUSAndNonUS) {
  // US Footnote
  GeminiConsentConfiguration* us_config =
      BuildStandardConfiguration(NO, NO, @"us");
  EXPECT_TRUE(
      HasLinkWithAction(us_config.footnote, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(us_config.footnote, kGeminiSecondFootnoteLinkAction));
  EXPECT_TRUE([us_config.footnote.string
      containsString:l10n_util::GetNSString(
                         IDS_IOS_BWG_CONSENT_FOOTNOTE_US_ONLY_ADDITION)]);

  // Korea Footnote
  GeminiConsentConfiguration* kr_config =
      BuildStandardConfiguration(NO, NO, @"kr");
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiKoreanTermsLinkAction));
  EXPECT_TRUE(
      HasLinkWithAction(kr_config.footnote, kGeminiSecondFootnoteLinkAction));
  EXPECT_FALSE([kr_config.footnote.string
      containsString:l10n_util::GetNSString(
                         IDS_IOS_BWG_CONSENT_FOOTNOTE_US_ONLY_ADDITION)]);
}

// Tests footnote strict watch link.
TEST_F(GeminiConsentConfigurationTest, StrictConsentFootnote) {
  GeminiConsentConfiguration* config =
      BuildStandardConfiguration(NO, YES, @"us");
  EXPECT_TRUE(HasLinkWithAction(config.footnote, kGeminiWatchLinkAction));
}
