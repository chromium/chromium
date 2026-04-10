// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/features.h"

BASE_FEATURE(kSnapshotInSwift, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSnapshotCompressedJPEGQuality,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSnapshotCompressedJPEGQualityEnabled() {
  return base::FeatureList::IsEnabled(kSnapshotCompressedJPEGQuality);
}

BASE_FEATURE(kSnapshotDownsampleImage, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSnapshotDownsampleImageEnabled() {
  return base::FeatureList::IsEnabled(kSnapshotDownsampleImage);
}
