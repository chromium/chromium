// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_metrics_recorder.h"

#import "base/test/metrics/histogram_tester.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class ComposeboxMetricsRecorderTest : public PlatformTest {
 protected:
  ComposeboxMetricsRecorderTest() {
    recorder_ = [[ComposeboxMetricsRecorder alloc] init];
  }

  ComposeboxMetricsRecorder* recorder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ComposeboxMetricsRecorderTest, AiModeActivationSource) {
  [recorder_ recordAiModeActivationSource:AiModeActivationSource::kToolMenu];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AiModeActivationSource",
      static_cast<int>(AiModeActivationSource::kToolMenu), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, AttachmentButtonShown) {
  [recorder_ recordAttachmentButtonShown:FuseboxAttachmentButtonType::kCamera];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AttachmentButtonShown",
      static_cast<int>(FuseboxAttachmentButtonType::kCamera), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, AttachmentButtonUsed) {
  [recorder_ recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kGallery];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AttachmentButtonUsed",
      static_cast<int>(FuseboxAttachmentButtonType::kGallery), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, AttachmentButtonsUsageInSession) {
  // Use some buttons
  [recorder_ recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kCamera];
  [recorder_ recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kFiles];

  // Record session usage
  [recorder_ recordAttachmentButtonsUsageInSession];

  // We expect to have the camera and files attachments recorded as used.
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.Camera", true, 1);
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.Files", true, 1);

  // We expect to have the Gallery button recorded as not used.
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.Gallery", false, 1);
}

TEST_F(ComposeboxMetricsRecorderTest, AttachmentsMenuShown) {
  [recorder_ recordAttachmentsMenuShown:YES];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AttachmentsPopupToggled", true, 1);
  [recorder_ recordAttachmentsMenuShown:NO];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AttachmentsPopupToggled", false, 1);
}

TEST_F(ComposeboxMetricsRecorderTest, TabPickerTabsAttached) {
  [recorder_ recordTabPickerTabsAttached:3];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.TabPickerTabsAttached", 3, 1);
}

TEST_F(ComposeboxMetricsRecorderTest, AutocompleteRequestTypeAtAbandon) {
  [recorder_
      recordAutocompleteRequestTypeAtAbandon:AutocompleteRequestType::kSearch];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AutocompleteRequestTypeAtAbandon",
      static_cast<int>(AutocompleteRequestType::kSearch), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, AutocompleteRequestTypeAtNavigation) {
  [recorder_ recordAutocompleteRequestTypeAtNavigation:AutocompleteRequestType::
                                                           kAIMode];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.MobileFusebox.AutocompleteRequestTypeAtNavigation",
      static_cast<int>(AutocompleteRequestType::kAIMode), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, FocusResultedInNavigation) {
  [recorder_ recordComposeboxFocusResultedInNavigation:NO
                                       withAttachments:NO
                                           requestType:AutocompleteRequestType::
                                                           kSearch];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.FocusResultedInNavigation",
      static_cast<int>(
          FocusResultedInNavigationType::kNoNavigationNoAttachments),
      1);
  histogram_tester_.ExpectBucketCount(
      "Omnibox.FocusResultedInNavigation.Search",
      static_cast<int>(
          FocusResultedInNavigationType::kNoNavigationNoAttachments),
      1);

  [recorder_ recordComposeboxFocusResultedInNavigation:YES
                                       withAttachments:NO
                                           requestType:AutocompleteRequestType::
                                                           kAIMode];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.FocusResultedInNavigation",
      static_cast<int>(FocusResultedInNavigationType::kNavigationNoAttachments),
      1);
  histogram_tester_.ExpectBucketCount(
      "Omnibox.FocusResultedInNavigation.AIMode",
      static_cast<int>(FocusResultedInNavigationType::kNavigationNoAttachments),
      1);

  [recorder_ recordComposeboxFocusResultedInNavigation:NO
                                       withAttachments:YES
                                           requestType:AutocompleteRequestType::
                                                           kImageGeneration];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.FocusResultedInNavigation",
      static_cast<int>(
          FocusResultedInNavigationType::kNoNavigationWithAttachments),
      1);
  histogram_tester_.ExpectBucketCount(
      "Omnibox.FocusResultedInNavigation.ImageGeneration",
      static_cast<int>(
          FocusResultedInNavigationType::kNoNavigationWithAttachments),
      1);

  [recorder_ recordComposeboxFocusResultedInNavigation:YES
                                       withAttachments:YES
                                           requestType:AutocompleteRequestType::
                                                           kSearch];
  histogram_tester_.ExpectBucketCount(
      "Omnibox.FocusResultedInNavigation",
      static_cast<int>(
          FocusResultedInNavigationType::kNavigationWithAttachments),
      1);

  histogram_tester_.ExpectTotalCount("Omnibox.FocusResultedInNavigation", 4);
}
