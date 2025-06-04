// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_coordinator.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_ui_handler.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@implementation SafariDataImportCoordinator {
  /// UI blocker used while the workflow is presenting. This makes sure that the
  /// promos manager would not attempt to show another promo in the meantime.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;
}

- (void)start {
  CHECK(base::FeatureList::IsEnabled(kImportPasswordsFromSafari));
  /// TODO(crbug.com/420694579): Implement actual logic before closing. Also
  /// move  UIBlocker to mediator.
  _UIBlocker = std::make_unique<ScopedUIBlocker>(self.browser->GetSceneState(),
                                                 UIBlockerExtent::kProfile);
  __weak SafariDataImportCoordinator* weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf.delegate safariImportWorkflowDidEndForCoordinator:weakSelf];
      }));
}

- (void)stop {
  /// TODO(crbug.com/420694579): Move to mediator.
  [self.UIHandler safariDataImportDidDismiss];
  _UIBlocker.reset();
}

@end
