// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONTENT_NOTIFICATION_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONTENT_NOTIFICATION_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@protocol NotificationsConfirmationPresenter;
@protocol SetUpListContentNotificationPromoCoordinatorDelegate;

// Enum actions for content notification promo action UMA metrics. Entries
// should not be renumbered and numeric values should never be reused. This
// should align with the ContentNotificationSetUpListPromoAction enum in
// enums.xml.
//
// LINT.IfChange
enum class ContentNotificationSetUpListPromoAction {
  kAccept = 0,
  kCancel = 1,
  kRemindMeLater = 2,
  kMaxValue = kRemindMeLater,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// Enum for content notification promo events UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationSetUpListPromoEvent enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationSetUpListPromoEvent {
  kShown = 0,
  kDismissed = 1,
  kPromptShown = 2,
  kMaxValue = kPromptShown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// Enum for content notification prompt actions UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationPromptAction enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationPromptAction {
  kGoToSettingsTapped = 0,
  kNoThanksTapped = 1,
  kAccepted = 2,
  kMaxValue = kAccepted,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// A coordinator that handles the display of the Content Notification Promo for
// the Set Up List.
@interface SetUpListContentNotificationPromoCoordinator
    : ChromeCoordinator <PromoStyleViewControllerDelegate,
                         UIAdaptivePresentationControllerDelegate>

// Creates a coordinator that uses `viewController` and `browser`. Uses
// `application` to open the app's settings.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               application:(UIApplication*)application
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The delegate that receives events from this coordinator.
@property(nonatomic, weak)
    id<SetUpListContentNotificationPromoCoordinatorDelegate>
        delegate;

// The presenter displays the notification confirmation message.
@property(nonatomic, weak) id<NotificationsConfirmationPresenter>
    messagePresenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONTENT_NOTIFICATION_PROMO_COORDINATOR_H_
