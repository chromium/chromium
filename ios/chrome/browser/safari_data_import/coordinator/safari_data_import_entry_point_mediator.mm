// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_entry_point_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"

@implementation SafariDataImportEntryPointMediator {
  /// UI blocker used while the workflow is presenting. This makes sure that the
  /// promos manager would not attempt to show another promo in the meantime.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;
  /// Promos manager that is used to register the reminder.
  raw_ptr<PromosManager> _promosManager;
  /// Feature engagement tracker used for the reminder.
  raw_ptr<feature_engagement::Tracker> _tracker;
  /// Whether the mediator has been disconnected.
  BOOL _disconnected;
}

- (instancetype)initWithUIBlockerTarget:(id<UIBlockerTarget>)target
                          promosManager:(PromosManager*)promosManager
               featureEngagementTracker:(feature_engagement::Tracker*)tracker {
  self = [super init];
  if (self) {
    CHECK(target);
    CHECK(promosManager);
    _UIBlocker =
        std::make_unique<ScopedUIBlocker>(target, UIBlockerExtent::kProfile);
    _promosManager = promosManager;
    _tracker = tracker;
  }
  return self;
}

- (void)dealloc {
  CHECK(_disconnected, base::NotFatalUntil::M143);
}

- (void)registerReminder {
  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::SafariImportRemindMeLater);
  _tracker->NotifyEvent(
      feature_engagement::events::kIOSSafariImportRemindMeLater);
}

- (void)notifyUsedOrDismissed {
  _tracker->NotifyUsedEvent(feature_engagement::kIPHiOSSafariImportFeature);
}

- (void)disconnect {
  _promosManager = nullptr;
  _UIBlocker.reset();
  _disconnected = YES;
}

@end
