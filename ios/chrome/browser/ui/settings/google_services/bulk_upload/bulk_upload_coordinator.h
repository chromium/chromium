// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol BulkUploadCoordinatorDelegate;

// Coordinator to save in account.
@interface BulkUploadCoordinator : ChromeCoordinator

// The delegate for the Save in Account.
@property(nonatomic, weak) id<BulkUploadCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_COORDINATOR_H_
