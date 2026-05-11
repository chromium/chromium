// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_strings.h"

#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

class ComposeboxStringsTest : public PlatformTest {
 protected:
  ComposeboxStringsTest() {}
};

TEST_F(ComposeboxStringsTest, TestLocalFallbackForTool) {
  ComposeboxStrings* strings = [ComposeboxStrings localFallbackStrings];

  // AIM
  EXPECT_NSEQ([strings menuLabelForTool:ComposeboxMode::kAIM],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION));
  EXPECT_NSEQ(
      [strings hintTextForTool:ComposeboxMode::kAIM],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ENABLED_PLACEHOLDER));

  // Image Gen
  EXPECT_NSEQ([strings menuLabelForTool:ComposeboxMode::kImageGeneration],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION));
  EXPECT_NSEQ([strings hintTextForTool:ComposeboxMode::kImageGeneration],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_IMAGE_GEN_PLACEHOLDER));

  // Canvas
  EXPECT_NSEQ([strings menuLabelForTool:ComposeboxMode::kCanvas],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION));
  EXPECT_NSEQ(
      [strings hintTextForTool:ComposeboxMode::kCanvas],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ENABLED_PLACEHOLDER));

  // Deep Search
  EXPECT_NSEQ([strings menuLabelForTool:ComposeboxMode::kDeepSearch],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION));
  EXPECT_NSEQ([strings hintTextForTool:ComposeboxMode::kDeepSearch],
              l10n_util::GetNSString(
                  IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ENABLED_PLACEHOLDER));
}

TEST_F(ComposeboxStringsTest, TestLocalFallbackForModel) {
  ComposeboxStrings* strings = [ComposeboxStrings localFallbackStrings];

  EXPECT_NSEQ(
      [strings menuLabelForModel:ComposeboxModelOption::kAuto],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO));
  EXPECT_NSEQ([strings menuLabelForModel:ComposeboxModelOption::kThinking],
              l10n_util::GetNSString(
                  IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_THINKING));
}

TEST_F(ComposeboxStringsTest, TestLocalFallbackForAttachment) {
  ComposeboxStrings* strings = [ComposeboxStrings localFallbackStrings];

  EXPECT_NSEQ(
      [strings
          stringForAttachmentOption:ComposeboxAttachmentOption::kCurrentTab],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_ADD_CURRENT_TAB_ACTION));
  EXPECT_NSEQ(
      [strings stringForAttachmentOption:ComposeboxAttachmentOption::kFile],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION));
}

TEST_F(ComposeboxStringsTest, TestServerStringsMapping) {
  std::unordered_map<ComposeboxMode, ComposeboxStringBundle*> tool_mapping;
  tool_mapping[ComposeboxMode::kImageGeneration] =
      [[ComposeboxStringBundle alloc] initWithMenuLabel:@"Server Menu Image"
                                              chipLabel:@"Server Chip Image"
                                               hintText:@"Server Hint Image"];

  ComposeboxStrings* strings =
      [[ComposeboxStrings alloc] initWithToolMapping:tool_mapping
                                        modelMapping:{}
                                  modelSectionHeader:@"Server Model Header"
                                  toolsSectionHeader:@"Server Tools Header"];

  EXPECT_NSEQ([strings menuLabelForTool:ComposeboxMode::kImageGeneration],
              @"Server Menu Image");
  EXPECT_NSEQ([strings chipLabelForTool:ComposeboxMode::kImageGeneration],
              @"Server Chip Image");
  EXPECT_NSEQ([strings hintTextForTool:ComposeboxMode::kImageGeneration],
              @"Server Hint Image");

  EXPECT_NSEQ([strings toolsSectionHeader], @"Server Tools Header");
  EXPECT_NSEQ([strings modelSectionHeader], @"Server Model Header");
}

TEST_F(ComposeboxStringsTest, TestHeaderFallbacks) {
  ComposeboxStrings* strings = [ComposeboxStrings localFallbackStrings];

  EXPECT_NSEQ(
      [strings toolsSectionHeader],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_TOOLS_SECTION_TITLE));
  EXPECT_NSEQ(
      [strings modelSectionHeader],
      l10n_util::GetNSStringF(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_TITLE, u"3"));
}
