// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_SWIFT_FEATURES_H_
#define IOS_CHROME_COMMON_SWIFT_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable new implementation of coordinators/mediators written
// in Swift.
BASE_DECLARE_FEATURE(kSwiftCoordinator);

// Returns true if new implementation written in Swift is enabled.
bool IsSwiftCoordinatorEnabled();

#endif  // IOS_CHROME_COMMON_SWIFT_FEATURES_H_
