// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_IMAGE_METADATA_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_IMAGE_METADATA_H_

// Represents a Lens metadata object.
// It is designed to be mostly opaque and used as a type safe mechanism of
// transferring information between Lens components at Chromium level.
@protocol LensImageMetadata <NSObject>

// Whether the image was capture with the camera.
@property(nonatomic, readonly) BOOL isCameraImage;

// TODO(crbug.com/400523059): Remove optional mark once roll is complete.
@optional
// Whether the filter mode is translate.
@property(nonatomic, readonly) BOOL translateFilterActive;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_IMAGE_METADATA_H_
