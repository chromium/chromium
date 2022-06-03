// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_LAYER_TREE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_LAYER_TREE_SETTINGS_H_

#include "cc/trees/layer_tree_settings.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

PLATFORM_EXPORT cc::ManagedMemoryPolicy GetGpuMemoryPolicy(
    const cc::ManagedMemoryPolicy& default_policy,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor);

cc::LayerTreeSettings GenerateLayerTreeSettings(
    bool has_compositor,
    bool for_child_local_root_frame,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_LAYER_TREE_SETTINGS_H_
