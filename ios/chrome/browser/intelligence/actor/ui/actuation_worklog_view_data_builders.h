// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_BUILDERS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_BUILDERS_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

@class ActuationHeaderContentVariants;
@class ActuationWorklogChip;
@class ActuationWorklogItem;
@class IntelligenceHeaderItem;

// Returns the chip displayed for `tool_type`, or `nil` if there is no tool or
// the tool doesn't display a chip.
ActuationWorklogChip* ActuationWorklogChipForToolType(
    std::optional<actor::ToolType> tool_type);

// Returns the header button stopping the task. Also used as the default when
// no header content is provided.
IntelligenceHeaderItem* ActuationHeaderCloseItem();

// Returns the header content of a task titled `task_title` in `task_state`.
// While the task runs, the minimized header shows `current_step` below the
// task title. Otherwise, a status message replaces the title and the task title
// moves below it. The primary button pauses a pausable task, and stops any
// other task.
ActuationHeaderContentVariants* ActuationHeaderContentVariantsForTask(
    NSString* task_title,
    actor::ActorTaskState task_state,
    NSString* current_step);

// TODO(crbug.com/586900659): Call the user response builders below from the
// user response flow.

// Returns the item showing that the user responded with `response`, which is
// either a typed message or the title of a tapped button. `response` must not
// be empty.
ActuationWorklogItem* ActuationWorklogItemForUserResponse(NSString* response);

// Returns the item showing that the task continues after a user response.
ActuationWorklogItem* ActuationWorklogItemForTaskContinues();

// Returns the item showing that the task ended.
ActuationWorklogItem* ActuationWorklogItemForTaskEnded();

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_DATA_BUILDERS_H_
