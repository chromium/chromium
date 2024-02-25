// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_COORDINATOR_DELEGATE_H_

@class BulkUploadCoordinator;

// Delegate for the BulkUpload Coordinator
@protocol BulkUploadCoordinatorDelegate <NSObject>

// Requests the delegate to stop `coordinator`.
- (void)bulkUploadCoordinatorShouldStop:(BulkUploadCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_COORDINATOR_DELEGATE_H_
