// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator protocol for user actions triggered within the actuation worklog.
@protocol ActuationWorklogMutator <NSObject>

// Requests stopping the active actuation task.
- (void)stopActuation;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_MUTATOR_H_
