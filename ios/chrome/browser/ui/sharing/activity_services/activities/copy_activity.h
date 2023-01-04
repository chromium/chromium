// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_COPY_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_COPY_ACTIVITY_H_

#import <UIKit/UIKit.h>

@class ShareToData;

// Activity that copies the URL to the pasteboard.
@interface CopyActivity : UIActivity

// Initializes the copy activity with the objects in `dataItems` holding URLs
// and potentially, additional text to be copied. `dataItems` must be non-null
// and not empty.
// `ShareToData.additionalText` will only be shared to the pasteboard if a
// single item is passed in `dataItems`. (When multiple items are passed, the
// URLs are made available in the pasteboard both as NSURLs and strings.)
- (instancetype)initWithDataItems:(NSArray<ShareToData*>*)dataItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_COPY_ACTIVITY_H_
