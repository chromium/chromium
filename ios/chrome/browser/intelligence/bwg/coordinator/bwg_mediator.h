// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"

class Browser;
class PrefService;

@protocol BWGMediatorDelegate;

// BWG Mediator.
@interface BWGMediator : NSObject <BWGConsentMutator>

- (instancetype)initWithPrefService:(PrefService*)prefService
                            browser:(Browser*)browser
                 baseViewController:(UIViewController*)baseViewController;

// The delegate for this mediator.
@property(nonatomic, weak) id<BWGMediatorDelegate> delegate;

// Presents the BWG flow, which can either show the FRE or BWG directly.
- (void)presentBWGFlow;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_H_
