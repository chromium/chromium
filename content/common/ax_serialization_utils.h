// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities to ensure that accessibility tree serialization and deserialization
// logic is in sync.

#ifndef CONTENT_COMMON_AX_SERIALIZATION_UTILS_H_
#define CONTENT_COMMON_AX_SERIALIZATION_UTILS_H_

namespace content {

// Returns true if page scale factor should be included in the transform on the
// root node of the AX tree.
bool AXShouldIncludePageScaleFactorInRoot();

}  // namespace content

#endif  // CONTENT_COMMON_AX_SERIALIZATION_UTILS_H_
