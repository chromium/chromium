// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_render_widget_host_view_for_stylus_writing.h"

namespace content {

bool MockRenderWidgetHostViewForStylusWriting::ShouldInitiateStylusWriting() {
  return supports_stylus_writing_;
}

void MockRenderWidgetHostViewForStylusWriting::NotifyHoverActionStylusWritable(
    bool stylus_writable) {
  hover_action_stylus_writable_ = stylus_writable;
}

}  // namespace content
