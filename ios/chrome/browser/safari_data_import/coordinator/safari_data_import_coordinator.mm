// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/passwords/model/features.h"

@implementation SafariDataImportCoordinator

- (void)start {
  CHECK(base::FeatureList::IsEnabled(kImportPasswordsFromSafari));
  // TODO(crbug.com/420694579): Implement actual logic before closing.
  [self.delegate safariImportWorkflowDidEndForCoordinator:self];
}

@end
