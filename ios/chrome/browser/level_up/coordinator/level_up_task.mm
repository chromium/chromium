// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"

@implementation LevelUpTask {
  raw_ptr<const TaskInfo> _taskInfo;
}

- (instancetype)initWithTaskInfo:(const TaskInfo*)taskInfo
                       completed:(BOOL)completed {
  self = [super init];
  if (self) {
    _taskInfo = taskInfo;
    _completed = completed;
  }
  return self;
}

#pragma mark - Computed Getters

- (NSString*)taskID {
  return base::SysUTF8ToNSString(TaskTypeToString(_taskInfo->GetTaskType()));
}

- (NSString*)title {
  return base::SysUTF8ToNSString(_taskInfo->GetTitle());
}

- (NSString*)taskDescription {
  return base::SysUTF8ToNSString(_taskInfo->GetTaskDescription());
}

- (Symbol)iconSymbol {
  return _taskInfo->GetIconSymbol();
}

- (LevelUpTaskCategory)category {
  return _taskInfo->GetCategory();
}

- (const TaskInfo*)taskInfo {
  return _taskInfo.get();
}

@end
