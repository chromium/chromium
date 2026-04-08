// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_

#ifdef __cplusplus
#import "base/feature_list.h"

// Feature flag to enable new snapshot system written in Swift.
BASE_DECLARE_FEATURE(kSnapshotInSwift);

// Feature flag to enable compressed JPEG quality (0.97) for snapshot images.
// When disabled (default), snapshots use quality 1.0 (no compression).
BASE_DECLARE_FEATURE(kSnapshotCompressedJPEGQuality);

// Feature flag to enable downsampling snapshot images to half resolution
// before writing to disk. When disabled (default), snapshots are written
// at their original captured resolution.
BASE_DECLARE_FEATURE(kSnapshotDownsampleImage);

extern "C" {
#endif  // __cplusplus

// Returns true if the kSnapshotCompressedJPEGQuality feature flag is enabled.
bool IsSnapshotCompressedJPEGQualityEnabled(void);

// Returns true if the kSnapshotDownsampleImage feature flag is enabled.
bool IsSnapshotDownsampleImageEnabled(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_
