// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/empty_cursor_filter.h"

#include <stdint.h>

#include <algorithm>

#include "build/build_config.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

protocol::CursorShapeInfo EmptyCursorShape() {
  protocol::CursorShapeInfo empty_shape;
  empty_shape.set_data(std::string());
  empty_shape.set_width(0);
  empty_shape.set_height(0);
  empty_shape.set_hotspot_x(0);
  empty_shape.set_hotspot_y(0);
  return empty_shape;
}

bool IsCursorShapeEmpty(const protocol::CursorShapeInfo& cursor_shape) {
  return cursor_shape.width() <= 0 || cursor_shape.height() <= 0;
}

EmptyCursorFilter::EmptyCursorFilter(protocol::CursorShapeStub* cursor_stub)
    : cursor_stub_(cursor_stub) {
}

EmptyCursorFilter::~EmptyCursorFilter() = default;

namespace {

#if defined(ARCH_CPU_LITTLE_ENDIAN)
const uint32_t kPixelAlphaMask = 0xff000000;
#else  // !defined(ARCH_CPU_LITTLE_ENDIAN)
const uint32_t kPixelAlphaMask = 0x000000ff;
#endif  // !defined(ARCH_CPU_LITTLE_ENDIAN)

// Returns true if |pixel| is not completely transparent.
bool IsVisiblePixel(uint32_t pixel) {
  return (pixel & kPixelAlphaMask) != 0;
}

// Returns true if there is at least one visible pixel in the given range.
bool IsVisibleRow(const uint32_t* begin, const uint32_t* end) {
  return std::any_of(begin, end, &IsVisiblePixel);
}

}  // namespace

void EmptyCursorFilter::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  const uint32_t* src_row_data = reinterpret_cast<const uint32_t*>(
      cursor_shape.data().data());
  const uint32_t* src_row_data_end =
      src_row_data + cursor_shape.width() * cursor_shape.height();
  if (IsVisibleRow(src_row_data, src_row_data_end)) {
    cursor_stub_->SetCursorShape(cursor_shape);
    return;
  }
  cursor_stub_->SetCursorShape(EmptyCursorShape());
}

}  // namespace remoting
