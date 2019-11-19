// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_image_group.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "media/gpu/android/codec_surface_bundle.h"

namespace media {

CodecImageGroup::CodecImageGroup(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<CodecSurfaceBundle> surface_bundle)
    : surface_bundle_(std::move(surface_bundle)),
      task_runner_(std::move(task_runner)) {
  // If the surface bundle has an overlay, then register for destruction
  // callbacks.  We thread-hop to the right thread, which means that we might
  // find out about destruction asynchronously.  Remember that the wp will be
  // cleared on |task_runner|.
  if (surface_bundle_->overlay()) {
    surface_bundle_->overlay()->AddSurfaceDestroyedCallback(base::BindOnce(
        [](scoped_refptr<base::SequencedTaskRunner> task_runner,
           base::OnceCallback<void(AndroidOverlay*)> cb,
           AndroidOverlay* overlay) -> void {
          task_runner->PostTask(FROM_HERE,
                                base::BindOnce(std::move(cb), overlay));
        },
        task_runner_,
        base::BindOnce(&CodecImageGroup::OnSurfaceDestroyed,
                       weak_this_factory_.GetWeakPtr())));
  }

  // TODO(liberato): if there's no overlay, should we clear |surface_bundle_|?
  // be sure not to call SurfaceDestroyed if !surface_bundle_ in that case when
  // adding a new image.
}

CodecImageGroup::~CodecImageGroup() {
  // Since every CodecImage should hold a strong ref to us until it becomes
  // unused, we shouldn't be destroyed with any outstanding images.
  DCHECK(images_.empty());
  CHECK(task_runner_->RunsTasksInCurrentSequence());
}

void CodecImageGroup::AddCodecImage(CodecImage* image) {
  // Temporary: crbug.com/986783 .
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  // If somebody adds an image after the surface has been destroyed, fail the
  // image immediately.  This can happen due to thread hopping.
  if (!surface_bundle_) {
    image->ReleaseCodecBuffer();
    return;
  }

  images_.insert(image);

  // Bind a strong ref to |this| so that the callback will prevent us from being
  // destroyed until the CodecImage is no longer in use for drawing.  In that
  // case, it doesn't need |surface_bundle_|, nor does it need to be notified
  // if the overlay is destroyed.
  image->AddUnusedCB(
      base::BindRepeating(&CodecImageGroup::OnCodecImageUnused, this));
}

void CodecImageGroup::OnCodecImageUnused(CodecImage* image) {
  // Temporary: crbug.com/986783 .
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  images_.erase(image);
}

void CodecImageGroup::OnSurfaceDestroyed(AndroidOverlay* overlay) {
  // Temporary: crbug.com/986783 .
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  // Release any codec buffer, so that the image doesn't try to render to the
  // overlay.  If it already did, that's fine.
  for (CodecImage* image : images_)
    image->ReleaseCodecBuffer();

  // While this might cause |surface_bundle_| to be deleted, it's okay because
  // it's a RefCountedDeleteOnSequence.
  surface_bundle_ = nullptr;
}

}  // namespace media
