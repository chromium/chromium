// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/impression_limit.h"

@implementation ImpressionLimit

- (instancetype)initWithLimit:(NSInteger)numImpressions
                   forNumDays:(NSInteger)numDays {
  if ((self = [super init])) {
    _numImpressions = numImpressions;
    _numDays = numDays;
  }

  return self;
}

@end
