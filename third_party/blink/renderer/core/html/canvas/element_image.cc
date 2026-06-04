// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/element_image.h"

#include <memory>
#include <utility>

namespace blink {

ElementImage::ElementImage(std::unique_ptr<CanvasChildPaintRecord> record)
    : record_(std::move(record)) {}

double ElementImage::width() const {
  if (!record_) {
    return 0;
  }
  return record_->paint_state.box_size.width();
}

double ElementImage::height() const {
  if (!record_) {
    return 0;
  }
  return record_->paint_state.box_size.height();
}

void ElementImage::close() {
  record_.reset();
}

std::unique_ptr<CanvasChildPaintRecord> ElementImage::TransferPaintRecord() {
  return std::move(record_);
}

}  // namespace blink
