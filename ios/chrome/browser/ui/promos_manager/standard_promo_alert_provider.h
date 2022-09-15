// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ALERT_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ALERT_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/promos_manager/promo_protocol.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_alert_handler.h"

// StandardPromoAlertProvider enables feature teams to simply and easily
// construct a promo alert for display by implementing the title and message
// below.
@protocol StandardPromoAlertProvider <PromoProtocol, StandardPromoAlertHandler>

@required

// The title of the alert.
- (NSString*)title;

// The message of the alert.
- (NSString*)message;

@optional

// The text for the "Default Action" button.
- (NSString*)defaultActionButtonText;

// The text for the "Cancel Action" button.
- (NSString*)cancelActionButtonText;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ALERT_PROVIDER_H_
