// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_compositor_dependencies.h"

#include <stddef.h>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/test_ukm_recorder_factory.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/buffer_types.h"

namespace content {

FakeCompositorDependencies::FakeCompositorDependencies() {
}

FakeCompositorDependencies::~FakeCompositorDependencies() {
}

bool FakeCompositorDependencies::IsUseZoomForDSFEnabled() {
  return use_zoom_for_dsf_;
}

}  // namespace content
