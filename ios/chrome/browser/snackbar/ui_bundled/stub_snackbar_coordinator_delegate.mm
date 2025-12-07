// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/ui_bundled/stub_snackbar_coordinator_delegate.h"

@class SnackbarCoordinator;

@implementation StubSnackbarCoordinatorDelegate

- (CGFloat)snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:
               (SnackbarCoordinator*)snackbarCoordinator
                                                forceBrowserToolbar:
                                                    (BOOL)forceBrowserToolbar {
  return 0;
}

@end
