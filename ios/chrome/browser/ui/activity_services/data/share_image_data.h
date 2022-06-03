// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_SHARE_IMAGE_DATA_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_SHARE_IMAGE_DATA_H_

#import <UIKit/UIKit.h>

// Data object used to represent an image that will be shared via the activity
// view.
@interface ShareImageData : NSObject

// Designated initializer.
- (instancetype)initWithImage:(UIImage*)image title:(NSString*)title;

// Image to be shared.
@property(nonatomic, readonly) UIImage* image;

// Title of the image to share.
@property(nonatomic, readonly, copy) NSString* title;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_SHARE_IMAGE_DATA_H_
