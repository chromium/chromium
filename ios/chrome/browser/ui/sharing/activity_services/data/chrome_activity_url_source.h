// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_URL_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_URL_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_source.h"

@class ChromeActivityItemThumbnailGenerator;

// This UIActivityItemSource-conforming object conforms to UTType public.url so
// it can be used with other Social Sharing Extensions as well. The `shareURL`
// is the URL shared with Social Sharing Extensions. The `subject` is used by
// Mail applications to pre-fill in the subject line.
@interface ChromeActivityURLSource : NSObject <ChromeActivityItemSource>

// Default initializer. `shareURL` and `subject` must not be nil.
- (instancetype)initWithShareURL:(NSURL*)shareURL subject:(NSString*)subject;

// Thumbnail generator used to provide thumbnails to extensions that request
// one.
@property(nonatomic, strong)
    ChromeActivityItemThumbnailGenerator* thumbnailGenerator;

// Prefillled link metadata that can be displayed in the share sheet.
@property(nonatomic, strong) LPLinkMetadata* linkMetadata;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_URL_SOURCE_H_
