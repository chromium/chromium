// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"

#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

class ComposeboxUIConfigTest : public PlatformTest {
 protected:
  ComposeboxUIConfigTest() {}
};

TEST_F(ComposeboxUIConfigTest, TestLocalFallbackForTool) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  // AIM
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kAIM],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION));
  EXPECT_NSEQ(
      [uiConfig hintTextForTool:ComposeboxMode::kAIM],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ENABLED_PLACEHOLDER));

  // Image Gen
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kImageGeneration],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION));
  EXPECT_NSEQ([uiConfig hintTextForTool:ComposeboxMode::kImageGeneration],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_IMAGE_GEN_PLACEHOLDER));

  // Canvas
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kCanvas],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION));
  EXPECT_NSEQ(
      [uiConfig hintTextForTool:ComposeboxMode::kCanvas],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ENABLED_PLACEHOLDER));

  // Deep Search
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kDeepSearch],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION));
  EXPECT_NSEQ([uiConfig hintTextForTool:ComposeboxMode::kDeepSearch],
              l10n_util::GetNSString(
                  IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ENABLED_PLACEHOLDER));
}

TEST_F(ComposeboxUIConfigTest, TestLocalFallbackForModel) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  EXPECT_NSEQ(
      [uiConfig menuLabelForModel:ComposeboxModelOption::kAuto],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO));
  EXPECT_NSEQ([uiConfig menuLabelForModel:ComposeboxModelOption::kThinking],
              l10n_util::GetNSString(
                  IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_THINKING));
}

TEST_F(ComposeboxUIConfigTest, TestLocalFallbackForAttachment) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  EXPECT_NSEQ(
      [uiConfig
          stringForAttachmentOption:ComposeboxAttachmentOption::kCurrentTab],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_ADD_CURRENT_TAB_ACTION));
  EXPECT_NSEQ(
      [uiConfig stringForAttachmentOption:ComposeboxAttachmentOption::kFile],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION));

  EXPECT_NSEQ(
      [uiConfig stringForAttachmentOption:ComposeboxAttachmentOption::kDrive],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DRIVE_ACTION));
}

TEST_F(ComposeboxUIConfigTest, TestServerStringsMapping) {
  std::unordered_map<ComposeboxMode, ComposeboxItemUIConfig*> tool_mapping;
  tool_mapping[ComposeboxMode::kImageGeneration] =
      [[ComposeboxItemUIConfig alloc] initWithMenuLabel:@"Server Menu Image"
                                              chipLabel:@"Server Chip Image"
                                               hintText:@"Server Hint Image"];

  ComposeboxUIConfig* uiConfig =
      [[ComposeboxUIConfig alloc] initWithToolMapping:tool_mapping
                                         modelMapping:{}
                                   modelSectionHeader:@"Server Model Header"
                                   toolsSectionHeader:@"Server Tools Header"];

  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kImageGeneration],
              @"Server Menu Image");
  EXPECT_NSEQ([uiConfig chipLabelForTool:ComposeboxMode::kImageGeneration],
              @"Server Chip Image");
  EXPECT_NSEQ([uiConfig hintTextForTool:ComposeboxMode::kImageGeneration],
              @"Server Hint Image");

  EXPECT_NSEQ([uiConfig toolsSectionHeader], @"Server Tools Header");
  EXPECT_NSEQ([uiConfig modelSectionHeader], @"Server Model Header");
}

TEST_F(ComposeboxUIConfigTest, TestHeaderFallbacks) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  EXPECT_NSEQ(
      [uiConfig toolsSectionHeader],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_TOOLS_SECTION_TITLE));
  EXPECT_NSEQ(
      [uiConfig modelSectionHeader],
      l10n_util::GetNSStringF(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_TITLE, u"3"));
}
