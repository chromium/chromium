// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_SCALE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_SCALE_H_

#import <UIKit/UIKit.h>

typedef enum {
  kImageScale1X,
  kImageScale2X,
} ImageScale;

@interface SnapshotImageScale : NSObject

// Returns the ImageScale that used for the current device.
+ (ImageScale)imageScaleForDevice;

// Returns the float value of the image scale for the current device.
+ (CGFloat)floatImageScaleForDevice;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_SCALE_H_
