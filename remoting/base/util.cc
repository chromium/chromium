// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/util.h"

#include <math.h>
#include <string.h>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "remoting/base/cpu_utils.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

constexpr int kBytesPerPixelRGB32 = 4;

static int CalculateRGBOffset(int x, int y, int stride) {
  return stride * y + kBytesPerPixelRGB32 * x;
}

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

void CopyRGB32Rect(const uint8_t* source_buffer,
                   int source_stride,
                   const webrtc::DesktopRect& source_buffer_rect,
                   uint8_t* dest_buffer,
                   int dest_stride,
                   const webrtc::DesktopRect& dest_buffer_rect,
                   const webrtc::DesktopRect& dest_rect) {
  DCHECK(DoesRectContain(dest_buffer_rect, dest_rect));
  DCHECK(DoesRectContain(source_buffer_rect, dest_rect));

  // Get the address of the starting point.
  source_buffer += CalculateRGBOffset(
      dest_rect.left() - source_buffer_rect.left(),
      dest_rect.top() - source_buffer_rect.top(), source_stride);
  dest_buffer += CalculateRGBOffset(dest_rect.left() - dest_buffer_rect.left(),
                                    dest_rect.top() - dest_buffer_rect.top(),
                                    source_stride);

  // Copy pixels in the rectangle line by line.
  const int bytes_per_line = kBytesPerPixelRGB32 * dest_rect.width();
  for (int i = 0; i < dest_rect.height(); ++i) {
    memcpy(dest_buffer, source_buffer, bytes_per_line);
    source_buffer += source_stride;
    dest_buffer += dest_stride;
  }
}

std::string ReplaceLfByCrLf(const std::string& in) {
  std::string out;
  out.resize(2 * in.size());
  char* out_p_begin = &out[0];
  char* out_p = out_p_begin;
  const char* in_p_begin = &in[0];
  const char* in_p_end = &in[in.size()];
  for (const char* in_p = in_p_begin; in_p < in_p_end; ++in_p) {
    char c = *in_p;
    if (c == '\n') {
      *out_p++ = '\r';
    }
    *out_p++ = c;
  }
  out.resize(out_p - out_p_begin);
  return out;
}

std::string ReplaceCrLfByLf(const std::string& in) {
  std::string out;
  out.resize(in.size());
  char* out_p_begin = &out[0];
  char* out_p = out_p_begin;
  const char* in_p_begin = &in[0];
  const char* in_p_end = &in[in.size()];
  for (const char* in_p = in_p_begin; in_p < in_p_end; ++in_p) {
    char c = *in_p;
    if ((c == '\r') && (in_p + 1 < in_p_end) && (*(in_p + 1) == '\n')) {
      *out_p++ = '\n';
      ++in_p;
    } else {
      *out_p++ = c;
    }
  }
  out.resize(out_p - out_p_begin);
  return out;
}

bool DoesRectContain(const webrtc::DesktopRect& a,
                     const webrtc::DesktopRect& b) {
  webrtc::DesktopRect intersection(a);
  intersection.IntersectWith(b);
  return intersection.equals(b);
}

}  // namespace remoting
