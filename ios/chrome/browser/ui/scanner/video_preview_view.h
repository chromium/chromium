// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCANNER_VIDEO_PREVIEW_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SCANNER_VIDEO_PREVIEW_VIEW_H_

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

// A subclass of UIView with the layerClass property set to
// AVCaptureVideoPreviewLayer. Contains the video preview for the QR scanner.
@interface VideoPreviewView : UIView

- (instancetype)initWithFrame:(CGRect)frame
                 viewportSize:(CGSize)previewViewportSize
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns the VideoPreviewView's layer cast to AVCaptureVideoPreviewLayer.
- (AVCaptureVideoPreviewLayer*)previewLayer;

// Returns the normalised rectangle of interest required for the Vision request.
- (CGRect)viewportRegionOfInterest;

// Returns the rectangle in camera coordinates in which codes should be
// recognized.
- (CGRect)viewportRectOfInterest;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCANNER_VIDEO_PREVIEW_VIEW_H_
