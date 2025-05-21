// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/gemini/ui/glic_consent_mutator.h"

class PrefService;

@protocol GLICMediatorDelegate;

// GLIC Mediator.
@interface GLICMediator : NSObject <GLICConsentMutator>

- (instancetype)initWithPrefService:(PrefService*)prefService;

// The delegate for this mediator.
@property(nonatomic, weak) id<GLICMediatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_H_
