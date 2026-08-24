// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

class ComposeboxUIConfigTest : public PlatformTest {
 protected:
  ComposeboxUIConfigTest() {}
};

// Tests that removeToolAccessibilityLabelForTool returns the correct localized
// string with placeholder replaced for fallback and server-configured tools.
TEST_F(ComposeboxUIConfigTest, TestRemoveToolAccessibilityLabel) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  EXPECT_NSEQ(
      [uiConfig removeToolAccessibilityLabelForTool:ComposeboxMode::kCanvas],
      l10n_util::GetNSStringF(
          IDS_IOS_COMPOSEBOX_REMOVE_TOOL_ACCESSIBILITY_LABEL,
          base::SysNSStringToUTF16(
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION))));

  EXPECT_NSEQ(
      [uiConfig
          removeToolAccessibilityLabelForTool:ComposeboxMode::kImageGeneration],
      l10n_util::GetNSStringF(
          IDS_IOS_COMPOSEBOX_REMOVE_TOOL_ACCESSIBILITY_LABEL,
          base::SysNSStringToUTF16(
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION))));

  EXPECT_NSEQ(
      [uiConfig
          removeToolAccessibilityLabelForTool:ComposeboxMode::kDeepSearch],
      l10n_util::GetNSStringF(
          IDS_IOS_COMPOSEBOX_REMOVE_TOOL_ACCESSIBILITY_LABEL,
          base::SysNSStringToUTF16(
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION))));

  std::unordered_map<ComposeboxMode, ComposeboxItemUIConfig*> tool_mapping;
  tool_mapping[ComposeboxMode::kCanvas] =
      [[ComposeboxItemUIConfig alloc] initWithMenuLabel:@"Server Menu Canvas"
                                              chipLabel:@"Server Canvas"
                                               hintText:@"Server Hint"
                                                   icon:nil];
  ComposeboxUIConfig* serverConfig =
      [[ComposeboxUIConfig alloc] initWithToolMapping:tool_mapping
                                         modelMapping:{}
                                   modelSectionHeader:nil
                                   toolsSectionHeader:nil];

  EXPECT_NSEQ([serverConfig
                  removeToolAccessibilityLabelForTool:ComposeboxMode::kCanvas],
              l10n_util::GetNSStringF(
                  IDS_IOS_COMPOSEBOX_REMOVE_TOOL_ACCESSIBILITY_LABEL,
                  base::SysNSStringToUTF16(@"Server Canvas")));
}

TEST_F(ComposeboxUIConfigTest, TestLocalFallbackForTool) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  // AIM
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kAIM],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION));
  EXPECT_NSEQ(
      [uiConfig hintTextForTool:ComposeboxMode::kAIM],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ENABLED_PLACEHOLDER));
  EXPECT_NE([uiConfig iconForTool:ComposeboxMode::kAIM], nil);

  // Image Gen
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kImageGeneration],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION));
  EXPECT_NSEQ([uiConfig hintTextForTool:ComposeboxMode::kImageGeneration],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_IMAGE_GEN_PLACEHOLDER));
  EXPECT_NE([uiConfig iconForTool:ComposeboxMode::kImageGeneration], nil);

  // Canvas
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kCanvas],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION));
  EXPECT_NSEQ(
      [uiConfig hintTextForTool:ComposeboxMode::kCanvas],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ENABLED_PLACEHOLDER));
  EXPECT_NE([uiConfig iconForTool:ComposeboxMode::kCanvas], nil);

  // Deep Search
  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kDeepSearch],
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION));
  EXPECT_NSEQ([uiConfig hintTextForTool:ComposeboxMode::kDeepSearch],
              l10n_util::GetNSString(
                  IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ENABLED_PLACEHOLDER));
  EXPECT_NE([uiConfig iconForTool:ComposeboxMode::kDeepSearch], nil);

  // Regular Search
  EXPECT_EQ([uiConfig iconForTool:ComposeboxMode::kRegularSearch], nil);
}

TEST_F(ComposeboxUIConfigTest, TestLocalFallbackForModel) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  EXPECT_NSEQ(
      [uiConfig menuLabelForModel:ComposeboxModelOption::kAuto],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO));
  EXPECT_NE([uiConfig iconForModel:ComposeboxModelOption::kAuto], nil);

  EXPECT_NSEQ([uiConfig menuLabelForModel:ComposeboxModelOption::kThinking],
              l10n_util::GetNSString(
                  IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_THINKING));
  EXPECT_NE([uiConfig iconForModel:ComposeboxModelOption::kThinking], nil);
  EXPECT_NE([uiConfig iconForModel:ComposeboxModelOption::kThinkingNoGenUI],
            nil);
  EXPECT_NE([uiConfig iconForModel:ComposeboxModelOption::kRegular], nil);
  EXPECT_NSEQ(
      [uiConfig menuLabelForModel:ComposeboxModelOption::kFlash],
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_FLASH));
  EXPECT_NE([uiConfig iconForModel:ComposeboxModelOption::kFlash], nil);
  EXPECT_EQ([uiConfig iconForModel:ComposeboxModelOption::kNone], nil);
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

TEST_F(ComposeboxUIConfigTest, TestServerConfigMapping) {
  UIImage* toolIcon = [[UIImage alloc] init];
  std::unordered_map<ComposeboxMode, ComposeboxItemUIConfig*> tool_mapping;
  tool_mapping[ComposeboxMode::kImageGeneration] =
      [[ComposeboxItemUIConfig alloc] initWithMenuLabel:@"Server Menu Image"
                                              chipLabel:@"Server Chip Image"
                                               hintText:@"Server Hint Image"
                                                   icon:toolIcon];

  UIImage* modelIcon = [[UIImage alloc] init];
  std::unordered_map<ComposeboxModelOption, ComposeboxItemUIConfig*>
      model_mapping;
  model_mapping[ComposeboxModelOption::kThinking] =
      [[ComposeboxItemUIConfig alloc] initWithMenuLabel:@"Server Menu Model"
                                              chipLabel:nil
                                               hintText:@"Server Hint Model"
                                                   icon:modelIcon];

  ComposeboxUIConfig* uiConfig =
      [[ComposeboxUIConfig alloc] initWithToolMapping:tool_mapping
                                         modelMapping:model_mapping
                                   modelSectionHeader:@"Server Model Header"
                                   toolsSectionHeader:@"Server Tools Header"];

  EXPECT_NSEQ([uiConfig menuLabelForTool:ComposeboxMode::kImageGeneration],
              @"Server Menu Image");
  EXPECT_NSEQ([uiConfig chipLabelForTool:ComposeboxMode::kImageGeneration],
              @"Server Chip Image");
  EXPECT_NSEQ([uiConfig hintTextForTool:ComposeboxMode::kImageGeneration],
              @"Server Hint Image");
  EXPECT_EQ([uiConfig iconForTool:ComposeboxMode::kImageGeneration], toolIcon);

  EXPECT_NSEQ([uiConfig menuLabelForModel:ComposeboxModelOption::kThinking],
              @"Server Menu Model");
  EXPECT_NSEQ([uiConfig hintTextForModel:ComposeboxModelOption::kThinking],
              @"Server Hint Model");
  EXPECT_EQ([uiConfig iconForModel:ComposeboxModelOption::kThinking],
            modelIcon);

  // Fallback for tools and models without server icon
  EXPECT_NE([uiConfig iconForTool:ComposeboxMode::kAIM], nil);
  EXPECT_NE([uiConfig iconForModel:ComposeboxModelOption::kAuto], nil);

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

TEST_F(ComposeboxUIConfigTest, TestUnknownToolAndModelFallback) {
  ComposeboxUIConfig* uiConfig = [ComposeboxUIConfig localFallbackUIConfig];

  // Unknown tool fallback returns a sensible icon and does not crash.
  ComposeboxMode unknownTool = static_cast<ComposeboxMode>(999);
  EXPECT_NE([uiConfig iconForTool:unknownTool], nil);

  // Unknown model fallback returns a sensible icon and does not crash.
  ComposeboxModelOption unknownModel = static_cast<ComposeboxModelOption>(999);
  EXPECT_NE([uiConfig iconForModel:unknownModel], nil);
}
