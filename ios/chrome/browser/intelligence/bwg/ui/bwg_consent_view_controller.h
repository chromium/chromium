// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_CONSENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_CONSENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_fre_view_controller_protocol.h"

@protocol BWGConsentMutator;

// BWG consent View Controller (VC).
@interface BWGConsentViewController
    : UIViewController <BWGFREViewControllerProtocol>

// Initializer for the VC whether the account is managed.
- (instancetype)initWithIsAccountManaged:(BOOL)isAccountManaged;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<BWGConsentMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_CONSENT_VIEW_CONTROLLER_H_
