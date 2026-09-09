// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"

#import "base/check.h"

namespace {

// Default total units of progress for a continued processing task.
constexpr int64_t kDefaultTotalUnits = 100;

}  // namespace

@implementation BackgroundContinuedProcessingTaskConfiguration

#pragma mark - Initializer

- (instancetype)initWithTitle:(NSString*)title
            expirationHandler:(ProceduralBlock)expirationHandler {
  CHECK(title.length > 0);
  CHECK(expirationHandler);

  if ((self = [super init])) {
    _title = [title copy];
    _expirationHandler = [expirationHandler copy];
    _totalUnits = kDefaultTotalUnits;
    if (@available(iOS 26.0, *)) {
      _strategy = BGContinuedProcessingTaskRequestSubmissionStrategyQueue;
      _requiredResources = BGContinuedProcessingTaskRequestResourcesDefault;
    }
  }
  return self;
}

@end
