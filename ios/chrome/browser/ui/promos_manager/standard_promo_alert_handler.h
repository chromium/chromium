// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ALERT_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ALERT_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol StandardPromoAlertHandler <NSObject>

@required

// The "Default Action" was touched.
- (void)standardPromoAlertDefaultAction;

@optional

// The "Cancel Action" button was touched.
- (void)standardPromoAlertCancelAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ALERT_HANDLER_H_
