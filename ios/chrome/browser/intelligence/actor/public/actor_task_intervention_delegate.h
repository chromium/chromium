// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_INTERVENTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_INTERVENTION_DELEGATE_H_

#import <Foundation/Foundation.h>

// The ActorTask intervention delegate protocol (1-to-1). Used for blocking
// interventions (likely user facing prompts, but can be programmatic in
// headless Actor mode).
@protocol ActorTaskInterventionDelegate <NSObject>

// TODO(crbug.com/501043031): Remove @optional when API stabilizes.
@optional

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_INTERVENTION_DELEGATE_H_
