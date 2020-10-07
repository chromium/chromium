// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/resource_load_stats.h"

#include "base/bind.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

namespace {

#if defined(OS_ANDROID)
void UpdateUserGestureCarryoverInfo(int render_frame_id) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (frame)
    frame->GetFrameHost()->UpdateUserGestureCarryoverInfo();
}
#endif

}  // namespace

#if defined(OS_ANDROID)
void NotifyUpdateUserGestureCarryoverInfo(int render_frame_id) {
  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    UpdateUserGestureCarryoverInfo(render_frame_id);
    return;
  }
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(UpdateUserGestureCarryoverInfo, render_frame_id));
}
#endif

}  // namespace content
