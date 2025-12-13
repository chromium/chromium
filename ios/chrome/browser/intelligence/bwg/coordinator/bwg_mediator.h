// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"

@protocol ApplicationCommands;
class BwgService;
class BwgBrowserAgent;
class PrefService;
class WebStateList;

@protocol BWGMediatorDelegate;

// BWG Mediator.
@interface BWGMediator : NSObject <BWGConsentMutator>

- (instancetype)initWithPrefService:(PrefService*)prefService
                       webStateList:(WebStateList*)webStateList
                 baseViewController:(UIViewController*)baseViewController
                         BWGService:(BwgService*)BWGService
                    BWGBrowserAgent:(BwgBrowserAgent*)BWGBrowserAgent
                            tracker:(feature_engagement::Tracker*)tracker;

// The delegate for this mediator.
@property(nonatomic, weak) id<BWGMediatorDelegate> delegate;

// The handler for sending application commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

// Presents the BWG flow, which can either show the FRE or BWG directly.
- (void)presentBWGFlow;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_H_
