// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_task.h"

@implementation LevelUpTask

- (instancetype)initWithTaskID:(NSString*)taskID
                         title:(NSString*)title
               taskDescription:(NSString*)taskDescription
                iconSymbolName:(NSString*)iconSymbolName
                     completed:(BOOL)completed
              navigationAction:(void (^)(void))navigationAction {
  self = [super init];
  if (self) {
    _taskID = [taskID copy];
    _title = [title copy];
    _taskDescription = [taskDescription copy];
    _iconSymbolName = [iconSymbolName copy];
    _completed = completed;
    _navigationAction = [navigationAction copy];
  }
  return self;
}

@end
