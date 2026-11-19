// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTUATION_WORKLOG_BUILDERS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTUATION_WORKLOG_BUILDERS_H_

#import <Foundation/Foundation.h>

@class ActuationWorklogItem;
@class ActuationWorklogState;
@protocol GeminiBrowserActuationsBlock;

// Returns the first item of a Gemini agent task worklog, telling the user that
// the task started.
ActuationWorklogItem* ActuationWorklogItemForTaskStarted();

// Returns the worklog of the completed agent task `block`, built from the data
// the Gemini SDK keeps for it: the task started item, then each action followed
// by the agent's message to the user after it, if any, then the task ended
// item. Consecutive actions with the same title show a single step, as in the
// live worklog, unless an agent message separates them.
ActuationWorklogState* ActuationWorklogStateForBrowserActuationsBlock(
    id<GeminiBrowserActuationsBlock> block);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTUATION_WORKLOG_BUILDERS_H_
