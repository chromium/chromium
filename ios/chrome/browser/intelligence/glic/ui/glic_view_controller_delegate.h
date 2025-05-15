// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_UI_GLIC_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_UI_GLIC_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class GLICConsentViewController;

// Declare the delegate protocol to communicate between the GLIC Consent VC and
// the GLIC NavigationController.
@protocol GLICConsentViewControllerDelegate <NSObject>

// Did accept the GLIC Promo.
- (void)didAcceptPromo;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_UI_GLIC_VIEW_CONTROLLER_DELEGATE_H_
