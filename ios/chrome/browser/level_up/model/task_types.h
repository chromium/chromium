// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_TYPES_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_TYPES_H_

#import <string>

// Enum for all available tasks in the Level Up feature.
enum class TaskType {
  kUnknown = 0,
  kTabGroups = 1,
  kAutofill = 2,
  kPinTabs = 3,
  kGemini = 4,
  kPaymentMethods = 5,
  kQuickDelete = 6,
  kSafeBrowsing = 7,
  kIncognito = 8,
  kPasswordCheckup = 9,
  kLensSearch = 10,
  kAISearch = 11,
  kCameraSearch = 12,
};

// Categories grouping the level-up tasks.
enum class LevelUpTaskCategory {
  // Tasks related to user productivity.
  kProductivity,
  // Tasks related to browsing safety.
  kSafety,
  // Tasks related to search integrations.
  kSearch,
};

// Returns a string representation of the TaskType.
std::string TaskTypeToString(TaskType type);

// Types representing the stats associated with completed tasks.
enum class LevelUpTaskStatType {
  // Number of tabs decluttered from grid.
  kTabsDecluttered,
  // Typing time saved by forms autofill.
  kTypingSaved,
  // Passwords verified by checkup.
  kPasswordsVerified,
  // Manual queries skipped by Lens.
  kSearchesSkipped,
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_TYPES_H_
