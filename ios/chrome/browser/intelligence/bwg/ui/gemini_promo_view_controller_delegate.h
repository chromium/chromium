// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_PROMO_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_PROMO_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Declare the delegate protocol to communicate between the Gemini Promo VC and
// the Gemini NavigationController.
@protocol GeminiPromoViewControllerDelegate <NSObject>

// Did accept the Gemini Promo.
- (void)didAcceptPromo;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_PROMO_VIEW_CONTROLLER_DELEGATE_H_
