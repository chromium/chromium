// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol PrivacyGuideURLUsageViewControllerDelegate;
@protocol PrivacyGuideViewControllerPresentationDelegate;

// View controller for the Privacy Guide URL usage step.
@interface PrivacyGuideURLUsageViewController
    : PromoStyleViewController <PrivacyGuideURLUsageConsumer>

// Presentation delegate.
@property(nonatomic, weak) id<PrivacyGuideViewControllerPresentationDelegate>
    presentationDelegate;

// Model delegate.
@property(nonatomic, weak) id<PrivacyGuideURLUsageViewControllerDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_VIEW_CONTROLLER_H_
