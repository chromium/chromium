// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_task_request.h"

#import "base/check.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"

@implementation BackgroundContinuedProcessingTaskRequest

- (instancetype)
    initWithIdentifier:(NSString*)identifier
         configuration:
             (BackgroundContinuedProcessingTaskConfiguration*)configuration
        startedHandler:
            (void (^)(BackgroundContinuedProcessingTaskContext* context))
                startedHandler {
  CHECK(identifier.length > 0);
  CHECK(configuration);
  if ((self = [super init])) {
    _identifier = [identifier copy];
    _configuration = configuration;
    _startedHandler = [startedHandler copy];
  }
  return self;
}

@end
