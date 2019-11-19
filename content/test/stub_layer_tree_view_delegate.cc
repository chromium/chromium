// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/stub_layer_tree_view_delegate.h"

#include <utility>

#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/swap_promise.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"

namespace content {

void StubLayerTreeViewDelegate::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  std::move(callback).Run(nullptr);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
StubLayerTreeViewDelegate::GetBeginMainFrameMetrics() {
  return nullptr;
}

}  // namespace content
