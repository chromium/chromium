// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_TOP_ALIGNED_IMAGE_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_TOP_ALIGNED_IMAGE_VIEW_H_

#import <UIKit/UIKit.h>

// The standard UIImageView zooms to the center of the image when it aspect
// fills. TopAlignedImageView aligns to the top, and fills the other edges.
// This means that an image narrower than the view gets zoomed and its bottom
// clipped, while a wider image than the view fits in height and is clipped on
// the left/right sides equally (i.e. the image stays horizontally centered).
// There is a special case though: when the image is portrait, the image view
// always fits the width. This is an unideal case, but relied upon by the grid
// transition animation to open a tab, where in Portrait mode, the snapshot is
// shorter than the image view but still needs to match in width to avoid a
// glitch when going from snapshot to the BVC.
@interface TopAlignedImageView : UIView

// The image displayed in the image view.
@property(nonatomic) UIImage* image;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_TOP_ALIGNED_IMAGE_VIEW_H_
