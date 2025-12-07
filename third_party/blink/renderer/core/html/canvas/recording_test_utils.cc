// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace blink_testing {

using ::cc::PaintOpEq;
using ::cc::RestoreOp;
using ::cc::SaveOp;

RecordedOpsView::RecordedOpsView(cc::PaintRecord record)
    : record_(std::move(record)), begin_(record_.begin()), end_(begin_) {
  CHECK_GE(record_.size(), 2u);

  // The first `PaintOp` must be a `SaveOp`.
  EXPECT_THAT(*begin_, PaintOpEq<SaveOp>());

  // Move `begin_` to the second element, and `end_` to the last, so tthat
  // iterating between `begin_` and `end_` will skip the last element.
  ++begin_;
  for (size_t i = 0; i < record_.size() - 1; ++i) {
    ++end_;
  }

  // The last `PaintOp` must be a `RestoreOp`.
  EXPECT_THAT(*end_, PaintOpEq<RestoreOp>());
}

cc::PaintFlags FillFlags() {
  cc::PaintFlags rect_flags;
  rect_flags.setAntiAlias(true);
  rect_flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
  return rect_flags;
}

cc::PaintFlags ClearRectFlags() {
  cc::PaintFlags clear_flags;
  clear_flags.setBlendMode(SkBlendMode::kClear);
  return clear_flags;
}

}  // namespace blink_testing
