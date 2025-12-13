// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_PROMO_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_PROMO_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Declare the delegate protocol to communicate between the BWG Promo VC and
// the BWG NavigationController.
@protocol BWGPromoViewControllerDelegate <NSObject>

// Did accept the BWG Promo.
- (void)didAcceptPromo;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_PROMO_VIEW_CONTROLLER_DELEGATE_H_
