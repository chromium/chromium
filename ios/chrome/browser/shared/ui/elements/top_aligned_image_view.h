// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_TOP_ALIGNED_IMAGE_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_TOP_ALIGNED_IMAGE_VIEW_H_

#import <UIKit/UIKit.h>

// The standard UIImageView zooms to the center of the image when it aspect
// fills. TopAlignedImageView aligns to the top of the image in all cases.
// When the image is portrait, the image matches the width of the view in all
// cases. When the image is landscape, the image matches the height of the view
// in all cases.
@interface TopAlignedImageView : UIView
// The image displayed in the image view.
@property(nonatomic) UIImage* image;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_TOP_ALIGNED_IMAGE_VIEW_H_
