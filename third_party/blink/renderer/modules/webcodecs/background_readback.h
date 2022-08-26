// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_BACKGROUND_READBACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_BACKGROUND_READBACK_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace blink {

class SyncReadbackThread;

// This class moves synchronous VideoFrame readback to a separate worker
// thread to avoid blocking the main thread.
class MODULES_EXPORT BackgroundReadback
    : public GarbageCollected<BackgroundReadback>,
      public Supplement<ExecutionContext> {
 public:
  using ReadbackDoneCallback =
      base::OnceCallback<void(scoped_refptr<media::VideoFrame>)>;

  explicit BackgroundReadback(base::PassKey<BackgroundReadback> key,
                              ExecutionContext& context);
  virtual ~BackgroundReadback();

  static const char kSupplementName[];
  static BackgroundReadback* From(ExecutionContext& context);

  void ReadbackTextureBackedFrameToMemory(
      scoped_refptr<media::VideoFrame> txt_frame,
      ReadbackDoneCallback result_cb);

  void Trace(Visitor* visitor) const override {
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  void ReadbackRGBTextureBackedFrameToMemory(
      scoped_refptr<media::VideoFrame> txt_frame,
      ReadbackDoneCallback result_cb);

  void ReadbackOnThread(scoped_refptr<media::VideoFrame> txt_frame,
                        ReadbackDoneCallback result_cb);

  void OnARGBPixelsReadCompleted(ReadbackDoneCallback result_cb,
                                 scoped_refptr<media::VideoFrame> txt_frame,
                                 scoped_refptr<media::VideoFrame> result_frame,
                                 bool success);

  // Lives and dies on the worker thread.
  scoped_refptr<SyncReadbackThread> sync_readback_impl_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;
  media::VideoFramePool result_frame_pool_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_BACKGROUND_READBACK_H_
