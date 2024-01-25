// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_HISTORY_SYNC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_HISTORY_SYNC_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol PrivacyGuideViewControllerPresentationDelegate;

// View controller for the Privacy Guide History Sync step.
@interface PrivacyGuideHistorySyncViewController : PromoStyleViewController

// Presentation delegate.
@property(nonatomic, weak) id<PrivacyGuideViewControllerPresentationDelegate>
    presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_HISTORY_SYNC_VIEW_CONTROLLER_H_
