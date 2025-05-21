// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_PROMO_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_PROMO_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Declare the delegate protocol to communicate between the GLIC Promo VC and
// the GLIC NavigationController.
@protocol GLICPromoViewControllerDelegate <NSObject>

// Did accept the GLIC Promo.
- (void)didAcceptPromo;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_PROMO_VIEW_CONTROLLER_DELEGATE_H_
