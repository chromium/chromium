// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/util.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "remoting/base/cpu_utils.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

// Do not write LOG messages in this routine since it is called from within
// our LOG message handler. Bad things will happen.
std::string GetTimestampString() {
  return base::UnlocalizedTimeFormatWithPattern(base::Time::NowFromSystemTime(),
                                                "MMdd/HHmmss:");
}

int RoundToTwosMultiple(int x) {
  return x & (~1);
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
  for (char c : in) {
    if (c == '\n') {
      out.push_back('\r');
    }
    out.push_back(c);
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
