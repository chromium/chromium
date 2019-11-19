// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/sync_compositor_messages.h"

namespace content {

SyncCompositorDemandDrawHwParams::SyncCompositorDemandDrawHwParams() {}

SyncCompositorDemandDrawHwParams::SyncCompositorDemandDrawHwParams(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority)
    : viewport_size(viewport_size),
      viewport_rect_for_tile_priority(viewport_rect_for_tile_priority),
      transform_for_tile_priority(transform_for_tile_priority) {}

SyncCompositorDemandDrawHwParams::~SyncCompositorDemandDrawHwParams() {}

SyncCompositorDemandDrawSwParams::SyncCompositorDemandDrawSwParams() {}

SyncCompositorDemandDrawSwParams::~SyncCompositorDemandDrawSwParams() {}

SyncCompositorCommonRendererParams::SyncCompositorCommonRendererParams() =
    default;

SyncCompositorCommonRendererParams::SyncCompositorCommonRendererParams(
    const SyncCompositorCommonRendererParams& other) = default;

SyncCompositorCommonRendererParams::~SyncCompositorCommonRendererParams() {}

SyncCompositorCommonRendererParams& SyncCompositorCommonRendererParams::
operator=(const SyncCompositorCommonRendererParams& other) = default;

}  // namespace content
