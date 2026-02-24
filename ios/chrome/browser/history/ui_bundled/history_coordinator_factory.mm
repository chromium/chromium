// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_coordinator_factory.h"

#import "build/config/ios/swift_buildflags.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_impl.h"

#if BUILDFLAG(ENABLE_SWIFT_CXX_INTEROP) && BUILDFLAG(HAS_SWIFT_6_3_SUPPORT)
#import "ios/chrome/browser/history/ui_bundled/swift_coordinator.h"  // nogncheck
#import "ios/chrome/common/swift/features.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_SWIFT_CXX_INTEROP) &&
        // BUILDFLAG(HAS_SWIFT_6_3_SUPPORT)

HistoryCoordinator* CreateHistoryCoordinator(UIViewController* view_controller,
                                             Browser* browser) {
#if BUILDFLAG(ENABLE_SWIFT_CXX_INTEROP) && BUILDFLAG(HAS_SWIFT_6_3_SUPPORT)
  if (IsSwiftCoordinatorEnabled()) {
    return [[SwiftHistoryCoordinatorImpl alloc]
        initWithBaseViewController:view_controller
                           browser:browser];
  }
#endif  // BUILDFLAG(ENABLE_SWIFT_CXX_INTEROP) &&
        // BUILDFLAG(HAS_SWIFT_6_3_SUPPORT)
  return
      [[HistoryCoordinatorImpl alloc] initWithBaseViewController:view_controller
                                                         browser:browser];
}
