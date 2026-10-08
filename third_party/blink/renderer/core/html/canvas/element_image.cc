// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/element_image.h"

#include <memory>
#include <utility>

namespace blink {

ElementImage::ElementImage(std::unique_ptr<CanvasDrawablePaintRecord> record)
    : record_(std::move(record)) {}

void ElementImage::close() {
  record_.reset();
}

std::unique_ptr<CanvasDrawablePaintRecord> ElementImage::TransferPaintRecord() {
  return std::move(record_);
}

DOMNodeId ElementImage::GetNodeId() const {
  return record_ ? record_->paint_state.canvas_drawable_node_id
                 : kInvalidDOMNodeId;
}

DOMNodeId ElementImage::GetCanvasNodeId() const {
  return record_ ? record_->paint_state.canvas_node_id : kInvalidDOMNodeId;
}

}  // namespace blink
