// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_INFOBAR_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_INFOBAR_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

// Values for the UMA Mobile.Messages.Translate.Banner.Event histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class MobileMessagesTranslateBannerEvent {
  // Translate was tapped.
  Translate = 0,
  // The banner was offering Show Original was tapped.
  ShowOriginal = 1,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = ShowOriginal,
};

// Values for the UMA Mobile.Messages.Translate.Modal.Event histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class MobileMessagesTranslateModalEvent {
  // User tapped on source language to see options to change it.
  ChangeSourceLanguage = 0,
  // User tapped on target language to see options to change it.
  ChangeTargetLanguage = 1,
  // The show original button was tapped.
  ShowOriginal = 2,
  // Always Translate was tapped.
  TappedAlwaysTranslate = 3,
  // Never Translate for the source language was tapped.
  TappedNeverForSourceLanguage = 4,
  // Never Translate for this site was tapped.
  TappedNeverForThisSite = 5,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = TappedNeverForThisSite,
};

// Values for the UMA Mobile.Messages.Translate.Modal.Present histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class MobileMessagesTranslateModalPresent {
  // The modal was presented after a prompt to Translate banner was
  // presented.
  PresentedAfterTranslatePromptBanner = 0,
  // The modal was presented after a Translate Finished banner was
  // presented.
  PresentedAfterTranslateFinishedBanner = 1,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = PresentedAfterTranslateFinishedBanner,
};

// This class records metrics for Translate-specific Messages events.
@interface TranslateInfobarMetricsRecorder : NSObject

// Records a histogram for `bannerEvent`.
+ (void)recordBannerEvent:(MobileMessagesTranslateBannerEvent)bannerEvent;
// Records a histogram for `event`.
+ (void)recordModalEvent:(MobileMessagesTranslateModalEvent)event;
// Records a histogram for `presentEvent`.
+ (void)recordModalPresent:(MobileMessagesTranslateModalPresent)presentEvent;
// Records a histogram for an infobar (both legacy and Messages) that the user
// did not interact with throughout its lifetime.
+ (void)recordUnusedInfobar;
@end

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_INFOBAR_METRICS_RECORDER_H_
