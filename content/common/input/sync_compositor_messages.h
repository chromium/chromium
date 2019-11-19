// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SYNC_COMPOSITOR_MESSAGES_H_
#define CONTENT_COMMON_SYNC_COMPOSITOR_MESSAGES_H_

#include <stddef.h>

#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/scroll_offset.h"

#ifndef INTERNAL_CONTENT_COMMON_SYNC_COMPOSITOR_MESSAGES_H_
#define INTERNAL_CONTENT_COMMON_SYNC_COMPOSITOR_MESSAGES_H_

namespace content {

struct SyncCompositorDemandDrawHwParams {
  SyncCompositorDemandDrawHwParams();
  SyncCompositorDemandDrawHwParams(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority);
  ~SyncCompositorDemandDrawHwParams();

  gfx::Size viewport_size;
  gfx::Rect clip;
  gfx::Rect viewport_rect_for_tile_priority;
  gfx::Transform transform_for_tile_priority;
};

struct SyncCompositorDemandDrawSwParams {
  SyncCompositorDemandDrawSwParams();
  ~SyncCompositorDemandDrawSwParams();

  gfx::Size size;
  gfx::Rect clip;
  gfx::Transform transform;
};

struct SyncCompositorCommonRendererParams {
  SyncCompositorCommonRendererParams();
  ~SyncCompositorCommonRendererParams();

  // Allow copy.
  SyncCompositorCommonRendererParams(
      const SyncCompositorCommonRendererParams& other);
  SyncCompositorCommonRendererParams& operator=(
      const SyncCompositorCommonRendererParams& other);

  unsigned int version = 0u;
  gfx::ScrollOffset total_scroll_offset;
  gfx::ScrollOffset max_scroll_offset;
  gfx::SizeF scrollable_size;
  float page_scale_factor = 0.f;
  float min_page_scale_factor = 0.f;
  float max_page_scale_factor = 0.f;
  bool need_animate_scroll = false;
  uint32_t need_invalidate_count = 0u;
  bool invalidate_needs_draw = true;
  uint32_t did_activate_pending_tree_count = 0u;
};

}  // namespace content

#endif  // INTERNAL_CONTENT_COMMON_SYNC_COMPOSITOR_MESSAGES_H_

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorDemandDrawHwParams)
  IPC_STRUCT_TRAITS_MEMBER(viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(viewport_rect_for_tile_priority)
  IPC_STRUCT_TRAITS_MEMBER(transform_for_tile_priority)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorDemandDrawSwParams)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(clip)
  IPC_STRUCT_TRAITS_MEMBER(transform)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorCommonRendererParams)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(total_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(max_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(scrollable_size)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(min_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(max_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(need_animate_scroll)
  IPC_STRUCT_TRAITS_MEMBER(need_invalidate_count)
  IPC_STRUCT_TRAITS_MEMBER(invalidate_needs_draw)
  IPC_STRUCT_TRAITS_MEMBER(did_activate_pending_tree_count)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_SYNC_COMPOSITOR_MESSAGES_H_
