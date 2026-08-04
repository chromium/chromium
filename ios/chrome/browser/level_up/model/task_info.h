// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_INFO_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_INFO_H_

#include <string>

#include "base/functional/callback.h"
#include "ios/chrome/browser/level_up/model/task_types.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@class CommandDispatcher;

// Interface that provides information about a task in the Level Up feature.
class TaskInfo {
 public:
  virtual ~TaskInfo();

  // The unique identifier for the task.
  virtual TaskType GetTaskType() const = 0;

  // The localized title of the task.
  virtual std::string GetTitle() const = 0;

  // The localized description of the task.
  virtual std::string GetTaskDescription() const = 0;

  // Icon symbol associated with the task.
  virtual Symbol GetIconSymbol() const = 0;

  // The category this task belongs to.
  virtual LevelUpTaskCategory GetCategory() const = 0;

  // The user action string that triggers completion of this task.
  virtual std::string GetTriggerUserAction() const = 0;

  // The localized completion snackbar message.
  virtual std::string GetCompletionSnackbarMessage() const = 0;

  // Callback to navigate the user to the task's entry point using the
  // dispatcher.
  using NavigationAction =
      base::RepeatingCallback<void(CommandDispatcher* dispatcher)>;
  virtual NavigationAction GetNavigationAction() const = 0;
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_INFO_H_
