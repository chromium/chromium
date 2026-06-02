// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"

@implementation LevelUpCategory

- (instancetype)initWithTitle:(NSString*)title
                        tasks:(NSArray<LevelUpTask*>*)tasks {
  self = [super init];
  if (self) {
    _title = title;
    _tasks = [tasks copy];
  }
  return self;
}

@end
