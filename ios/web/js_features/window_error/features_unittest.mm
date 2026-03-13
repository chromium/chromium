// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/features.h"

#import <string_view>

#import "base/test/scoped_feature_list.h"
#import "testing/platform_test.h"

namespace {
constexpr std::string_view kUnexpectedErrorSignature =
    "Unexpected error message missing api name.";
constexpr std::string_view kCrWebErrorSignature =
    "CrWebError: API namespace is not registered in CrWeb. (namespace.api)";
constexpr std::string_view kReferenceErrorSignature =
    "ReferenceError: Can't find variable: xyz (namespace_foo.bar)";
}  // namespace

namespace web {

typedef PlatformTest IOSJavaScriptErrorReportMessageFilterTest;

// Tests that all reports are sent when filtering is disabled.
TEST_F(IOSJavaScriptErrorReportMessageFilterTest, FilterDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kIOSJavaScriptErrorReportSignatureFilter);

  EXPECT_TRUE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kUnexpectedErrorSignature));
  EXPECT_TRUE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kCrWebErrorSignature));
  EXPECT_TRUE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kReferenceErrorSignature));
}

// Tests that no reports are sent when filtering is enabled with no regex param
// specified. (Default param value should match all messages.)
TEST_F(IOSJavaScriptErrorReportMessageFilterTest, FilterEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kIOSJavaScriptErrorReportSignatureFilter);

  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kUnexpectedErrorSignature));
  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kCrWebErrorSignature));
  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kReferenceErrorSignature));
}

// Tests that no reports are sent when the filtering regex param is invalid.
TEST_F(IOSJavaScriptErrorReportMessageFilterTest, InvalidRegexParam) {
  base::FieldTrialParams params;
  params["regex"] = "^\\";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kIOSJavaScriptErrorReportSignatureFilter, params);

  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kUnexpectedErrorSignature));
  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kCrWebErrorSignature));
  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kReferenceErrorSignature));
}

// Tests report filtering based on error message prefix.
TEST_F(IOSJavaScriptErrorReportMessageFilterTest, RegexErrorMessageFiltering) {
  base::FieldTrialParams params;
  params["regex"] = "^CrWebError:[\\s\\S]*";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kIOSJavaScriptErrorReportSignatureFilter, params);

  // Error signatures matching the regex filter must not be sent.
  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kCrWebErrorSignature));

  // Error signatures not matching the regex filter will be sent.
  EXPECT_TRUE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kUnexpectedErrorSignature));
  EXPECT_TRUE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kReferenceErrorSignature));
}

// Tests report filtering based on API name.
TEST_F(IOSJavaScriptErrorReportMessageFilterTest, RegexAPIFiltering) {
  base::FieldTrialParams params;
  // params["regex"] = "\\(namespace\\.api\\)$";
  params["regex"] = "[\\s\\S]*\\(namespace_foo\\.bar\\)$";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kIOSJavaScriptErrorReportSignatureFilter, params);

  // Error signatures matching the regex filter must not be sent.
  EXPECT_FALSE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kReferenceErrorSignature));

  // Error signatures not matching the regex filter will be sent.
  EXPECT_TRUE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kUnexpectedErrorSignature));
  EXPECT_TRUE(AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
      kCrWebErrorSignature));
}

}  // namespace web
