// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_coordinator_factory.h"

#import "ios/chrome/browser/history/ui_bundled/history_coordinator_impl.h"

HistoryCoordinator* CreateHistoryCoordinator(UIViewController* view_controller,
                                             Browser* browser) {
  return
      [[HistoryCoordinatorImpl alloc] initWithBaseViewController:view_controller
                                                         browser:browser];
}
