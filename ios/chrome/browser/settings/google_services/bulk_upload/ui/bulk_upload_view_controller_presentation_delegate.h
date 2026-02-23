// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_UI_BULK_UPLOAD_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_UI_BULK_UPLOAD_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class BulkUploadViewController;

// Delegate for the BulkUploadviewController
@protocol BulkUploadViewControllerPresentationDelegate <NSObject>

// Requests the delegate to dismiss `controller`.
- (void)bulkUploadViewControllerWantsToBeDismissed:
    (BulkUploadViewController*)controller;

// Requests the delegate to dismiss `controller`.
- (void)bulkUploadViewControllerIsBeingDismissed:
    (BulkUploadViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_UI_BULK_UPLOAD_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
