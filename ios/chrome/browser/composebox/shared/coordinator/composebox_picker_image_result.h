// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_IMAGE_RESULT_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_IMAGE_RESULT_H_

#import <Foundation/Foundation.h>

// Represents a picked image result.
@interface ComposeboxPickerImageResult : NSObject

/// The image provider object.
@property(nonatomic, readonly) NSItemProvider* imageProvider;
/// The asset identifier, used for deduplication.
@property(nonatomic, copy, readonly) NSString* assetID;

// Creates a new object of this type.
- (instancetype)initWithImageProvider:(NSItemProvider*)imageProvider
                              assetID:(NSString*)assetID;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_IMAGE_RESULT_H_
