// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ActuationInterventionData;

// Consumer protocol for receiving actuation intervention updates.
@protocol ActuationInterventionConsumer <NSObject>

// Displays an interactive intervention, or dismisses any active intervention
// if `intervention` is nil. No-op if `intervention` is already displayed.
- (void)setIntervention:(ActuationInterventionData*)intervention;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_CONSUMER_H_
