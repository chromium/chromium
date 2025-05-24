// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace set_up_list {

// Accessibility IDs used for various UI items.
extern NSString* const kSignInItemID;
extern NSString* const kDefaultBrowserItemID;
extern NSString* const kAutofillItemID;
extern NSString* const kContentNotificationItemID;
extern NSString* const kAllSetItemID;
extern NSString* const kFollowItemID;
extern NSString* const kAccessibilityID;
extern NSString* const kExpandButtonID;
extern NSString* const kMenuButtonID;
extern NSString* const kAllSetID;
extern NSString* const kSetUpListContainerID;

}  // namespace set_up_list

// UMA Default Browser Histogram name.
// This histogram records which action a user takes on the Static and Animated
// versions of the Set Up List Default Browser Promo.
extern const char kSetUpListDefaultBrowserPromoAction[];

// Enum for the IOS.SetUpList.DefaultBrowser.Action histogram.
// Keep in sync with `IOSSegmentedDefaultBrowserPromoAction`
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange(SegmentedDefaultBrowserPromoAction)
enum class SegmentedDefaultBrowserPromoAction {
  kStaticPromoAppear = 0,  // The Static Default Browser Promo was displayed.
  kStaticPromoAccept =
      1,  // The user accepted the Static Default Browser Promo.
  kStaticPromoDismiss =
      2,  // The user dismissed the Static Default Browser Promo.
  kAnimatedPromoAppear =
      3,  // The Animated Default Browser Promo was displayed.
  kAnimatedPromoAccept =
      4,  // The user accepted the Animated Default Browser Promo.
  kAnimatedPromoDismiss =
      5,  // The user dismissed the Animated Default Browser Promo.
  kMaxValue = kAnimatedPromoDismiss,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSegmentedDefaultBrowserPromoAction)

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_CONSTANTS_H_
