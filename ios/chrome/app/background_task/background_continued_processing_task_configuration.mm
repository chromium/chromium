// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"

#import "base/check.h"
#import "base/check_op.h"

@implementation BackgroundContinuedProcessingTaskConfiguration

#pragma mark - Initializer

- (instancetype)initWithTitle:(NSString*)title
            expirationHandler:(ProceduralBlock)expirationHandler {
  CHECK(title.length > 0);
  CHECK(expirationHandler);

  if ((self = [super init])) {
    _title = [title copy];
    _expirationHandler = [expirationHandler copy];
    _totalUnits = kDefaultTotalUnitsOfProgress;
    _expectedStepCount = kDefaultExpectedStepCount;
    if (@available(iOS 26.0, *)) {
      _strategy = BGContinuedProcessingTaskRequestSubmissionStrategyQueue;
      _requiredResources = BGContinuedProcessingTaskRequestResourcesDefault;
    }
  }
  return self;
}

#pragma mark - Properties

- (void)setTitle:(NSString*)title {
  CHECK(title.length > 0);
  _title = [title copy];
}

- (void)setTotalUnits:(int64_t)totalUnits {
  CHECK_GT(totalUnits, 0);
  _totalUnits = totalUnits;
}

- (void)setExpectedStepCount:(int64_t)expectedStepCount {
  CHECK_GT(expectedStepCount, 0);
  _expectedStepCount = expectedStepCount;
}

@end
