// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_CONSUMER_H_

#import <UIKit/UIKit.h>
#import <vector>

@class BulkUploadViewItem;

// Consumer for the BulkUpload view controller
@protocol BulkUploadConsumer <NSObject>

// Sets the enabled state of the button.
- (void)setValidationButtonEnabled:(BOOL)enabled;

// Update the view controller about new bookmark data.
- (void)updateViewWithViewItems:(NSArray<BulkUploadViewItem*>*)viewItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_CONSUMER_H_
