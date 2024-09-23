// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"

using password_manager::PasswordManagerSurface;

@implementation IOSPasswordManagerVisitsRecorder {
  // The surface for which visits are logged.
  PasswordManagerSurface _surface;

  // Whether the metric counting visits to the page was already recorded.
  // Used to avoid over-recording the metric after each successful
  // authentication.
  BOOL _visitRecorded;
}

- (instancetype)initWithPasswordManagerSurface:
    (password_manager::PasswordManagerSurface)surface {
  if ((self = [super init])) {
    _surface = surface;
  }
  return self;
}

- (void)maybeRecordVisitMetric {
  if (_visitRecorded) {
    return;
  }
  _visitRecorded = YES;
  password_manager::LogPasswordManagerSurfaceVisit(_surface);
}

@end
