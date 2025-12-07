// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_IMAGE_VIEW_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_IMAGE_VIEW_H_

#import <UIKit/UIKit.h>

@class HomeCustomizationFramingCoordinates;

/// Updates `desired_frame` based on the provided parameters. If the orientation
/// of the frame does not match the orientation of the view, then the frame is
/// rotated. And then if the frame goes outside the bounds of the original
/// image, it is scaled down around its center.
CGRect UpdateDesiredFrame(CGRect desired_frame,
                          BOOL orientation_matches,
                          CGSize image_size);

// View to show all types of customization images, both gallery and
// user-provided.
@interface HomeCustomizationImageView : UIView

// The current image displayed in the view.
@property(nonatomic, readonly) UIImage* image;

// Sets the image to be displayed by this view, along with the framing
// coordinates, if available, to use to position the image.
- (void)setImage:(UIImage*)image
    framingCoordinates:(HomeCustomizationFramingCoordinates*)framingCoordinates;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_IMAGE_VIEW_H_
