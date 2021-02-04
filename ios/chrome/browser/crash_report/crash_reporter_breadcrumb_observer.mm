// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_reporter_breadcrumb_observer.h"

#include "base/strings/sys_string_conversions.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CrashReporterBreadcrumbObserver () {
  // Map associating the observed BreadcrumbManager with the corresponding
  // observer bridge instances.
  std::map<BreadcrumbManager*, std::unique_ptr<BreadcrumbManagerObserverBridge>>
      _breadcrumbManagerObservers;

  // Map associating the observed BreadcrumbManagerKeyedServices with the
  // corresponding observer bridge instances.
  std::map<BreadcrumbManagerKeyedService*,
           std::unique_ptr<BreadcrumbManagerObserverBridge>>
      _breadcrumbManagerServiceObservers;

  // A string which stores the received breadcrumbs. Since breakpad limits
  // product data string length, it may be truncated when a new event is added
  // in order to reduce overall memory usage.
  NSMutableString* _breadcrumbs;
}

// Updates the breadcrumbs stored in the crash log.
- (void)updateBreadcrumbEventsCrashKey;

@end

@implementation CrashReporterBreadcrumbObserver

+ (CrashReporterBreadcrumbObserver*)uniqueInstance {
  static CrashReporterBreadcrumbObserver* instance =
      [[CrashReporterBreadcrumbObserver alloc] init];
  return instance;
}

- (instancetype)init {
  if ((self = [super init])) {
    _breadcrumbs = [[NSMutableString alloc] init];
  }
  return self;
}

- (void)observeBreadcrumbManager:(BreadcrumbManager*)breadcrumbManager {
  DCHECK(!_breadcrumbManagerObservers[breadcrumbManager]);

  _breadcrumbManagerObservers[breadcrumbManager] =
      std::make_unique<BreadcrumbManagerObserverBridge>(breadcrumbManager,
                                                        self);
}

- (void)stopObservingBreadcrumbManager:(BreadcrumbManager*)breadcrumbManager {
  _breadcrumbManagerObservers.erase(breadcrumbManager);
}

- (void)observeBreadcrumbManagerService:
    (BreadcrumbManagerKeyedService*)breadcrumbManagerService {
  DCHECK(!_breadcrumbManagerServiceObservers[breadcrumbManagerService]);

  _breadcrumbManagerServiceObservers[breadcrumbManagerService] =
      std::make_unique<BreadcrumbManagerObserverBridge>(
          breadcrumbManagerService, self);
}

- (void)stopObservingBreadcrumbManagerService:
    (BreadcrumbManagerKeyedService*)breadcrumbManagerService {
  _breadcrumbManagerServiceObservers[breadcrumbManagerService] = nullptr;
}

- (void)setPreviousSessionEvents:(const std::vector<std::string>&)events {
  for (auto event_it = events.rbegin(); event_it != events.rend(); ++event_it) {
    NSString* event = base::SysUTF8ToNSString(*event_it);
    NSString* eventWithSeperator = [NSString stringWithFormat:@"%@\n", event];
    [_breadcrumbs appendString:eventWithSeperator];
  }

  [self updateBreadcrumbEventsCrashKey];
}

- (void)updateBreadcrumbEventsCrashKey {
  if (_breadcrumbs.length > breadcrumbs::kMaxDataLength) {
    NSRange trimRange =
        NSMakeRange(breadcrumbs::kMaxDataLength,
                    _breadcrumbs.length - breadcrumbs::kMaxDataLength);
    [_breadcrumbs deleteCharactersInRange:trimRange];
  }

  crash_keys::SetBreadcrumbEvents(_breadcrumbs);
}

#pragma mark - BreadcrumbManagerObserving protocol

- (void)breadcrumbManager:(BreadcrumbManager*)manager
              didAddEvent:(NSString*)event {
  NSString* eventWithSeperator = [NSString stringWithFormat:@"%@\n", event];
  [_breadcrumbs insertString:eventWithSeperator atIndex:0];

  [self updateBreadcrumbEventsCrashKey];
}

@end
