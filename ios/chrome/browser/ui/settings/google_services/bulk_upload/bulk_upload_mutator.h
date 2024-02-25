// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_MUTATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_constants.h"

@protocol BulkUploadMutator <NSObject>

// Change the enabled status of sync of bookmarks.
- (void)bulkUploadViewItemWithType:(BulkUploadType)index
                        isSelected:(BOOL)selected;
// Bulk upload the data. Authenticate the user first if there are passwords to
// upload.
- (void)requestSave;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_MUTATOR_H_
