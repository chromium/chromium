// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view_utils.h"

#import <cmath>

CGSize GetWatermarkExpandedSizeForRotation(CGSize size, CGFloat angle) {
  // Calculates the required size of the replicators to cover the view bounds
  // after rotation.
  //
  // Consider a rectangle ABCD with width W and height H before rotation:
  //
  //   D (0, H)                      C (W, H)
  //     +---------------------------+
  //     |                           |
  //   H |                           |
  //     |                           |
  //     +---------------------------+
  //   A (0, 0)      W               B (W, 0)
  //
  // Now rotate this rectangle counter-clockwise by angle θ around the origin
  // A(0,0)
  // clang-format off
  //                                           Y
  //                                           ^
  //                                           |   (W cos θ - H sin θ, W sin θ + H cos θ)
  //   ^                                       |                 + C'
  //   |                                       |               /  \
  //   |                                       |           /       \
  //   |                                       |       /            \
  //   |                                       |   /                 \
  //   |                                       |                      \
  //   |                                   /   |                       \
  //   |        D' (-H sin θ, H cos θ) +       |                        \
  //   H'                             \        |                         \
  //   |                               \       |                          \
  //   |                                \      |                           + B'
  //   |                                 \     |                       / (W cos θ, W sin θ)
  //   |                                  \    |                   /
  //   |                                   \   |               /
  //   |                                    \  |           /
  //   |                                     \ |       /
  //   |                                      \|   /  θ degrees
  // --v---------------------------------------+------------------------------------------- X
  //                                       A (0,0)
  //                                   |<--------------- W' -------------->|
  // clang-format on
  // The minimum bounding box enclosing the rotated rectangle has width W'
  // and height H':
  //   W' = W * |cos θ| + H * |sin θ|
  //   H' = H * |cos θ| + W * |sin θ|
  //
  // Thus, to cover a viewport of size W x H after rotating the grid by θ, the
  // unrotated grid must be expanded to size W' x H' before the rotation
  // transform is applied.
  CGFloat expandedWidth = size.width * std::abs(std::cos(angle)) +
                          size.height * std::abs(std::sin(angle));
  CGFloat expandedHeight = size.height * std::abs(std::cos(angle)) +
                           size.width * std::abs(std::sin(angle));
  return CGSizeMake(expandedWidth, expandedHeight);
}
