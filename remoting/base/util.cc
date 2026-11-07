// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/util.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

#include "remoting/base/cpu_utils.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

int RoundToTwosMultiple(int x) {
  return x & (~1);
}

webrtc::DesktopSize ScaleSizeToFit(const webrtc::DesktopSize& size,
                                   const webrtc::DesktopSize& max_size) {
  if (size.is_empty() || max_size.is_empty()) {
    return webrtc::DesktopSize();
  }

  int64_t width = size.width();
  int64_t height = size.height();
  int64_t max_width = max_size.width();
  int64_t max_height = max_size.height();

  if (width <= max_width && height <= max_height) {
    return size;
  }

  // Compare scale factors (max_width / width) vs (max_height / height) via
  // cross-multiplication to avoid floating-point inaccuracy.
  if (max_width * height <= max_height * width) {
    int32_t scaled_height =
        std::max<int64_t>(1, (height * max_width + width / 2) / width);
    return webrtc::DesktopSize(max_width, scaled_height);
  }

  int32_t scaled_width =
      std::max<int64_t>(1, (width * max_height + height / 2) / height);
  return webrtc::DesktopSize(scaled_width, max_height);
}

webrtc::DesktopRect AlignRect(const webrtc::DesktopRect& rect) {
  int x = RoundToTwosMultiple(rect.left());
  int y = RoundToTwosMultiple(rect.top());
  int right = RoundToTwosMultiple(rect.right() + 1);
  int bottom = RoundToTwosMultiple(rect.bottom() + 1);
  return webrtc::DesktopRect::MakeLTRB(x, y, right, bottom);
}

webrtc::DesktopRect GetRowAlignedRect(const webrtc::DesktopRect rect,
                                      int max_right) {
  static const int align = GetSimdMemoryAlignment();
  static const int align_mask = ~(align - 1);
  int new_left = (rect.left() & align_mask);
  int new_right = std::min((rect.right() + align - 1) & align_mask, max_right);
  return webrtc::DesktopRect::MakeLTRB(new_left, rect.top(), new_right,
                                       rect.bottom());
}

std::string ReplaceLfByCrLf(std::string_view in) {
  std::string out;
  out.reserve(2 * in.size());
  for (char ch : in) {
    if (ch == '\n') {
      out.push_back('\r');
    }
    out.push_back(ch);
  }
  return out;
}

std::string ReplaceCrLfByLf(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size();) {
    if (i + 1 < in.size() && in[i] == '\r' && in[i + 1] == '\n') {
      out.push_back('\n');
      i += 2;
    } else {
      out.push_back(in[i]);
      ++i;
    }
  }
  return out;
}

bool DoesRectContain(const webrtc::DesktopRect& a,
                     const webrtc::DesktopRect& b) {
  webrtc::DesktopRect intersection(a);
  intersection.IntersectWith(b);
  return intersection.equals(b);
}

}  // namespace remoting
