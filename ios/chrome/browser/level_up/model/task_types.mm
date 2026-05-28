// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/task_types.h"

std::string TaskTypeToString(TaskType type) {
  switch (type) {
    case TaskType::kUnknown:
      return "Unknown";
    case TaskType::kTabGroups:
      return "TabGroups";
  }
}
