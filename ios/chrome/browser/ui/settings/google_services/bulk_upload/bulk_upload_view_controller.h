// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol BulkUploadMutator;
@protocol BulkUploadViewControllerPresentationDelegate;

@interface BulkUploadViewController : UIViewController <BulkUploadConsumer>

@property(nonatomic, weak) id<BulkUploadViewControllerPresentationDelegate>
    delegate;
@property(nonatomic, weak) id<BulkUploadMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_VIEW_CONTROLLER_H_
