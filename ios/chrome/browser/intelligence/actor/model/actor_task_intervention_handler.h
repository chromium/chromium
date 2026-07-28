// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_INTERVENTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_INTERVENTION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"

// Implementation of ActorTaskInterventionDelegate that automatically selects
// the first suggestion.
//
// TODO(crbug.com/496195979): This is a temporary placeholder. Remove when the
// real one is implemented.
@interface ActorTaskInterventionHandler
    : NSObject <ActorTaskInterventionDelegate>

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_INTERVENTION_HANDLER_H_
