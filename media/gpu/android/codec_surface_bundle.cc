// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_surface_bundle.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/android/android_overlay.h"

namespace media {

CodecSurfaceBundle::CodecSurfaceBundle()
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunnerHandle::Get()) {}

CodecSurfaceBundle::CodecSurfaceBundle(std::unique_ptr<AndroidOverlay> overlay)
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunnerHandle::Get()),
      overlay_(std::move(overlay)) {}

CodecSurfaceBundle::CodecSurfaceBundle(
    scoped_refptr<gpu::TextureOwner> texture_owner)
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunnerHandle::Get()),
      codec_buffer_wait_coordinator_(
          base::MakeRefCounted<CodecBufferWaitCoordinator>(
              std::move(texture_owner))),
      texture_owner_surface_(codec_buffer_wait_coordinator_->texture_owner()
                                 ->CreateJavaSurface()) {}

CodecSurfaceBundle::~CodecSurfaceBundle() {
  // Explicitly free the surface first, just to be sure that it's deleted before
  // the TextureOwner is.
  texture_owner_surface_ = gl::ScopedJavaSurface();

  // Also release the back buffers.
  if (!codec_buffer_wait_coordinator_)
    return;

  auto task_runner =
      codec_buffer_wait_coordinator_->texture_owner()->task_runner();
  if (task_runner->RunsTasksInCurrentSequence()) {
    codec_buffer_wait_coordinator_->texture_owner()->ReleaseBackBuffers();
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindRepeating(&gpu::TextureOwner::ReleaseBackBuffers,
                            codec_buffer_wait_coordinator_->texture_owner()));
  }
}

const base::android::JavaRef<jobject>& CodecSurfaceBundle::GetJavaSurface()
    const {
  return overlay_ ? overlay_->GetJavaSurface()
                  : texture_owner_surface_.j_surface();
}

CodecSurfaceBundle::ScheduleLayoutCB CodecSurfaceBundle::GetScheduleLayoutCB() {
  return base::BindRepeating(&CodecSurfaceBundle::ScheduleLayout,
                             weak_factory_.GetWeakPtr());
}

void CodecSurfaceBundle::ScheduleLayout(const gfx::Rect& rect) {
  if (layout_rect_ == rect)
    return;
  layout_rect_ = rect;

  if (overlay_)
    overlay_->ScheduleLayout(rect);
}

}  // namespace media
