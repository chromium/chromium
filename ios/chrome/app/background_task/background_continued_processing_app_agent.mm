// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_app_agent.h"

#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"

@implementation BackgroundContinuedProcessingAppAgent

#pragma mark - Public

- (BackgroundContinuedProcessingTaskContext*)
    requestTaskWithIdentifier:(NSString*)identifier
                configuration:(BackgroundContinuedProcessingTaskConfiguration*)
                                  configuration {
  // TODO(crbug.com/532206258): Implement.
  return nil;
}

@end
