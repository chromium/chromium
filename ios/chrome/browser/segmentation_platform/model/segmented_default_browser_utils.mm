// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#import "components/segmentation_platform/public/constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"

namespace segmentation_platform {

const base::TimeDelta kDeviceSwitcherWaitTimeout = base::Seconds(1);

DefaultBrowserUserSegment GetDefaultBrowserUserSegment(
    const ClassificationResult* device_switcher_result,
    const ClassificationResult* shopper_result) {
  std::string forced_device_switcher_label =
      experimental_flags::GetSegmentForForcedDeviceSwitcherExperience();
  std::string forced_shopper_label =
      experimental_flags::GetSegmentForForcedShopperExperience();

  // If an experience is forced via the iOS Experimental Settings or in Chrome's
  // command line flags, return the corresponding segment. Used for manual and
  // automated testing.
  if (forced_device_switcher_label == DeviceSwitcherModel::kDesktopLabel) {
    return DefaultBrowserUserSegment::kDesktopUser;
  }
  if (forced_device_switcher_label == DeviceSwitcherModel::kAndroidPhoneLabel) {
    return DefaultBrowserUserSegment::kAndroidSwitcher;
  }
  if (forced_shopper_label == kShoppingUserUmaName) {
    return DefaultBrowserUserSegment::kShopper;
  }

  if (device_switcher_result &&
      device_switcher_result->status == PredictionStatus::kSucceeded) {
    if (std::find(device_switcher_result->ordered_labels.begin(),
                  device_switcher_result->ordered_labels.end(),
                  DeviceSwitcherModel::kDesktopLabel) !=
        device_switcher_result->ordered_labels.end()) {
      return DefaultBrowserUserSegment::kDesktopUser;
    }
    if (std::find(device_switcher_result->ordered_labels.begin(),
                  device_switcher_result->ordered_labels.end(),
                  DeviceSwitcherModel::kAndroidPhoneLabel) !=
        device_switcher_result->ordered_labels.end()) {
      return DefaultBrowserUserSegment::kAndroidSwitcher;
    }
  }

  if (shopper_result &&
      shopper_result->status == PredictionStatus::kSucceeded) {
    // A shopper segment classification result is binary, `ordered_labels`
    // should only have one label.
    if (std::find(shopper_result->ordered_labels.begin(),
                  shopper_result->ordered_labels.end(), kShoppingUserUmaName) !=
        shopper_result->ordered_labels.end()) {
      return DefaultBrowserUserSegment::kShopper;
    }
  }
  return DefaultBrowserUserSegment::kDefault;
}

int GetFirstRunDefaultBrowserScreenTitleStringID(
    DefaultBrowserUserSegment segment) {
  switch (segment) {
    case DefaultBrowserUserSegment::kDesktopUser:
    case DefaultBrowserUserSegment::kAndroidSwitcher:
      return UseIPadTailoredStringForDefaultBrowserPromo()
                 ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPAD
                 : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPHONE;
    case DefaultBrowserUserSegment::kShopper:
      return IDS_IOS_SEGMENTED_DEFAULT_BROWSER_SCREEN_SHOPPER_TITLE;
    case DefaultBrowserUserSegment::kDefault:
      return UseIPadTailoredStringForDefaultBrowserPromo()
                 ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                 : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE;
  }
  NOTREACHED();
}

int GetFirstRunDefaultBrowserScreenSubtitleStringID(
    DefaultBrowserUserSegment segment) {
  switch (segment) {
    case DefaultBrowserUserSegment::kDesktopUser:
      return UseIPadTailoredStringForDefaultBrowserPromo()
                 ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPAD
                 : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPHONE;
    case DefaultBrowserUserSegment::kAndroidSwitcher:
      return UseIPadTailoredStringForDefaultBrowserPromo()
                 ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPAD
                 : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPHONE;
    case DefaultBrowserUserSegment::kShopper:
      return IDS_IOS_SEGMENTED_DEFAULT_BROWSER_SCREEN_SHOPPER_SUBTITLE;
    case DefaultBrowserUserSegment::kDefault:
      return UseIPadTailoredStringForDefaultBrowserPromo()
                 ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                 : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE;
  }
  NOTREACHED();
}

int GetDefaultBrowserGenericPromoTitleStringID(
    DefaultBrowserUserSegment segment) {
  switch (segment) {
    case DefaultBrowserUserSegment::kDesktopUser:
    case DefaultBrowserUserSegment::kAndroidSwitcher:
      return IDS_IOS_SEGMENTED_DEFAULT_BROWSER_VIDEO_PROMO_DEVICE_SWITCHER_TITLE;
    case DefaultBrowserUserSegment::kShopper:
      return IDS_IOS_SEGMENTED_DEFAULT_BROWSER_VIDEO_PROMO_SHOPPER_TITLE;
    case DefaultBrowserUserSegment::kDefault:
      return IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_TITLE_TEXT;
  }
  NOTREACHED();
}

int GetSetUpListDefaultBrowserDescriptionStringID(
    DefaultBrowserUserSegment segment) {
  switch (segment) {
    case DefaultBrowserUserSegment::kDesktopUser:
    case DefaultBrowserUserSegment::kAndroidSwitcher:
      return IDS_IOS_SET_UP_LIST_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_SHORT_DESCRIPTION;
    case DefaultBrowserUserSegment::kShopper:
      return IDS_IOS_SET_UP_LIST_SEGMENTED_DEFAULT_BROWSER_SHOPPER_SHORT_DESCRIPTION;
    case DefaultBrowserUserSegment::kDefault:
      return IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_SHORT_DESCRIPTION;
  }
  NOTREACHED();
}

}  // namespace segmentation_platform
