// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

const PaintLayer* HitTestRequest::GetStopLayer() const {
  if (stop_node_ && !stop_layer_) {
    stop_layer_ = stop_node_->PaintingLayer();
  }
  return stop_layer_.Get();
}

void HitTestRequest::Trace(Visitor* visitor) const {
  visitor->Trace(stop_node_);
  visitor->Trace(stop_layer_);
}

}  // namespace blink
