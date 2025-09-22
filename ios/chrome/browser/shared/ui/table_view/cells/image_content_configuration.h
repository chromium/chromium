// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_IMAGE_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_IMAGE_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// A content configuration for an image view.
@interface ImageContentConfiguration : NSObject <UIContentConfiguration>

// LINT.IfChange(Copy)

// The image to be displayed.
@property(nonatomic, strong) UIImage* image;

// The size of the image.
@property(nonatomic, assign) CGSize imageSize;

// The content mode of the image. Default is UIViewContentModeScaleAspectFit.
@property(nonatomic, assign) UIViewContentMode imageContentMode;

// LINT.ThenChange(image_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_IMAGE_CONTENT_CONFIGURATION_H_
