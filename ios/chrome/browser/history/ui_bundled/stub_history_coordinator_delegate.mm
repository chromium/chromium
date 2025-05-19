// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/stub_history_coordinator_delegate.h"

@implementation StubHistoryCoordinatorDelegate

#pragma mark - HistoryCoordinatorDelegate

- (void)closeHistoryWithCompletion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }
}

- (void)closeHistory {
  // This method is required as part of `HistoryCoordinatorDelegate` but
  // is a noop since this is a stub to be used in tests.
}

@end
