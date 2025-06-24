// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"

@implementation SafariDataImportMainMediator {
  /// UI blocker used while the workflow is presenting. This makes sure that the
  /// promos manager would not attempt to show another promo in the meantime.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;
  /// Promos manager that is used to register the reminder.
  raw_ptr<PromosManager> _promosManager;
}

- (instancetype)initWithUIBlockerTarget:(id<UIBlockerTarget>)target
                          promosManager:(PromosManager*)promosManager {
  self = [super init];
  if (self) {
    CHECK(target);
    CHECK(promosManager);
    _UIBlocker =
        std::make_unique<ScopedUIBlocker>(target, UIBlockerExtent::kProfile);
    _promosManager = promosManager;
  }
  return self;
}

- (void)registerReminder {
  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::SafariImportRemindMeLater);
}

- (void)disconnect {
  _promosManager = nullptr;
  _UIBlocker.reset();
}

@end
