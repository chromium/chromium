// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_UTIL_H_
#define REMOTING_BASE_UTIL_H_

#include <string>
#include <string_view>

#include "build/build_config.h"

namespace webrtc {
class DesktopRect;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

int RoundToTwosMultiple(int x);

// Scales `size` down proportionally, if necessary, to fit within `max_size`
// while preserving aspect ratio. Returns an empty DesktopSize if either `size`
// or `max_size` is empty.
webrtc::DesktopSize ScaleSizeToFit(const webrtc::DesktopSize& size,
                                   const webrtc::DesktopSize& max_size);

// Align the sides of the rectangle to multiples of 2 (expanding outwards).
webrtc::DesktopRect AlignRect(const webrtc::DesktopRect& rect);

// Widens the left and right X-coordinates of `rect` (in pixels, clipped to
// `max_right`) to multiples of `GetSimdMemoryAlignment()`. Because 8-bit planar
// YUV stores 1 byte per luma/I444 sample, aligning X-bounds by the CPU's SIMD
// byte alignment ensures both destination plane byte offsets and row pixel
// widths satisfy libyuv's full-vector SIMD kernel alignment requirements.
webrtc::DesktopRect GetRowAlignedRect(const webrtc::DesktopRect rect,
                                      int max_right);

// Replaces every occurrence of "\n" in a string by "\r\n".
std::string ReplaceLfByCrLf(std::string_view in);

// Replaces every occurrence of "\r\n" in a string by "\n".
std::string ReplaceCrLfByLf(std::string_view in);

bool DoesRectContain(const webrtc::DesktopRect& a,
                     const webrtc::DesktopRect& b);

}  // namespace remoting

#endif  // REMOTING_BASE_UTIL_H_
