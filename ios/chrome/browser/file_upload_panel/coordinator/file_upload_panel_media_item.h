// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIA_ITEM_H_
#define IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIA_ITEM_H_

#import <Foundation/Foundation.h>

// Data object that represents a selected media item.
@interface FileUploadPanelMediaItem : NSObject
// The URL of the file.
@property(nonatomic, copy) NSURL* fileURL;
// Whether the file is a video.
@property(nonatomic, assign) BOOL isVideo;
- (instancetype)initWithFileURL:(NSURL*)fileURL
                        isVideo:(BOOL)isVideo NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIA_ITEM_H_
