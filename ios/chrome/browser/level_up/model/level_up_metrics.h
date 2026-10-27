// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_METRICS_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_METRICS_H_

enum class TaskType;

// UMA histogram names.
inline constexpr char kLevelUpTaskStartedHistogram[] =
    "IOS.LevelUp.TaskStarted";
inline constexpr char kLevelUpTaskCompletedHistogram[] =
    "IOS.LevelUp.TaskCompleted";
inline constexpr char kLevelUpNotificationsStateChangedHistogram[] =
    "IOS.LevelUp.NotificationsStateChanged";
inline constexpr char kLevelUpNewTaskNotificationsStateChangedHistogram[] =
    "IOS.LevelUp.NewTaskNotificationsStateChanged";
inline constexpr char kLevelUpLevelChangedHistogram[] =
    "IOS.LevelUp.LevelChanged";
inline constexpr char kLevelUpCurrentLevelHistogram[] =
    "IOS.LevelUp.CurrentLevel";
inline constexpr char kLevelUpOptInStateChangedHistogram[] =
    "IOS.LevelUp.OptInStateChanged";

// Records that the user started the given `task_type` from the Level Up UI.
void RecordLevelUpTaskStarted(TaskType task_type);

// Records that the user completed the given `task_type`.
void RecordLevelUpTaskCompleted(TaskType task_type);

// Records that the user changed the Level Up notifications (progress updates)
// state to `enabled`.
void RecordLevelUpNotificationsStateChanged(bool enabled);

// Records that the user changed the Level Up new task notifications state to
// `enabled`.
void RecordLevelUpNewTaskNotificationsStateChanged(bool enabled);

// Records that the user's Level Up level changed to `level`.
void RecordLevelUpLevelChanged(int level);

// Records the opted-in user's current Level Up `level` when the scene becomes
// active.
void RecordLevelUpCurrentLevel(int level);

// Records that the user opted in (`opted_in` = true) or opted out (`opted_in` =
// false) of Level Up.
void RecordLevelUpOptInStateChanged(bool opted_in);

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_METRICS_H_
