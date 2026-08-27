// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASKS_TASK_FACTORIES_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASKS_TASK_FACTORIES_H_

#include <memory>

class TaskInfo;

std::unique_ptr<TaskInfo> CreateTabGroupsTaskInfo();
std::unique_ptr<TaskInfo> CreateAutofillTaskInfo();
std::unique_ptr<TaskInfo> CreatePinTabsTaskInfo();
std::unique_ptr<TaskInfo> CreateGeminiTaskInfo();
std::unique_ptr<TaskInfo> CreatePaymentMethodsTaskInfo();
std::unique_ptr<TaskInfo> CreateQuickDeleteTaskInfo();
std::unique_ptr<TaskInfo> CreateSafeBrowsingTaskInfo();
std::unique_ptr<TaskInfo> CreateIncognitoTaskInfo();
std::unique_ptr<TaskInfo> CreatePasswordCheckupTaskInfo();
std::unique_ptr<TaskInfo> CreateLensWebsiteSearchTaskInfo();
std::unique_ptr<TaskInfo> CreateAISearchTaskInfo();
std::unique_ptr<TaskInfo> CreateLensCameraSearchTaskInfo();

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASKS_TASK_FACTORIES_H_
