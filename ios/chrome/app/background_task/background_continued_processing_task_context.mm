// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "base/check.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"

@implementation BackgroundContinuedProcessingTaskContext

#pragma mark - Initializer

- (instancetype)initWithTaskIdentifier:(NSString*)taskIdentifier
                         configuration:
                             (BackgroundContinuedProcessingTaskConfiguration*)
                                 configuration
                         finishHandler:(ProceduralBlock)finishHandler {
  CHECK(taskIdentifier.length > 0);
  CHECK(configuration);
  if ((self = [super init])) {
    // TODO(crbug.com/532206258): Implement.
  }
  return self;
}

#pragma mark - Public

- (double)fractionCompleted {
  // TODO(crbug.com/532206258): Implement.
  return 0.0;
}

- (BOOL)isCompleted {
  // TODO(crbug.com/532206258): Implement.
  return NO;
}

- (void)updateTitle:(NSString*)title subtitle:(NSString*)subtitle {
  // TODO(crbug.com/532206258): Implement.
}

- (void)incrementProgressByUnits:(int64_t)units {
  // TODO(crbug.com/532206258): Implement.
}

- (void)setCompletedUnits:(int64_t)completedUnits {
  // TODO(crbug.com/532206258): Implement.
}

- (void)setTaskCompletedWithSuccess:(BOOL)success {
  // TODO(crbug.com/532206258): Implement.
}

#pragma mark - Internal

- (void)attachUnderlyingTask:(BGContinuedProcessingTask*)task
    API_AVAILABLE(ios(26.0)) {
  // TODO(crbug.com/532206258): Implement.
}

@end
