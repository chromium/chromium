// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_BACKGROUND_READBACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_BACKGROUND_READBACK_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_frame_pool.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace blink {

class SyncReadbackThread;

// This class moves synchronous VideoFrame readback to a separate worker
// thread to avoid blocking the main thread.
class MODULES_EXPORT BackgroundReadback
    : public GarbageCollected<BackgroundReadback>,
      public Supplement<ExecutionContext> {
 public:
  using ReadbackToFrameDoneCallback =
      base::OnceCallback<void(scoped_refptr<media::VideoFrame>)>;
  using ReadbackDoneCallback = base::OnceCallback<void(bool)>;

  explicit BackgroundReadback(base::PassKey<BackgroundReadback> key,
                              ExecutionContext& context);
  virtual ~BackgroundReadback();

  static const char kSupplementName[];
  static BackgroundReadback* From(ExecutionContext& context);

  // Copies mapped CPU/memory plane data from `src_frame` into `dest_frame`.
  // Returns true on success, or false if formats or sizes do not match.
  static bool CopyMappablePlanes(const media::VideoFrame& src_frame,
                                 media::VideoFrame& dest_frame);

  void ReadbackTextureBackedFrameToMemoryFrame(
      scoped_refptr<media::VideoFrame> txt_frame,
      ReadbackToFrameDoneCallback result_cb);

  void ReadbackTextureBackedFrame(scoped_refptr<media::VideoFrame> txt_frame,
                                  scoped_refptr<media::VideoFrame> dest_frame,
                                  ReadbackDoneCallback done_cb);

  bool ReadbackTextureBackedFrameSync(media::VideoFrame& txt_frame,
                                      media::VideoFrame& dest_frame);

  void Trace(Visitor* visitor) const override {
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  void ReadbackOnThread(scoped_refptr<media::VideoFrame> txt_frame,
                        ReadbackToFrameDoneCallback result_cb);

  void ReadbackOnThread(scoped_refptr<media::VideoFrame> txt_frame,
                        scoped_refptr<media::VideoFrame> dest_frame,
                        ReadbackDoneCallback done_cb);

  void ReadbackRGBTextureBackedFrameToMemory(
      scoped_refptr<media::VideoFrame> txt_frame,
      ReadbackToFrameDoneCallback result_cb);

  void OnARGBPixelsFrameReadCompleted(
      ReadbackToFrameDoneCallback result_cb,
      scoped_refptr<media::VideoFrame> txt_frame,
      scoped_refptr<media::VideoFrame> result_frame,
      bool success);

  void ReadbackRGBTextureBackedFrame(
      scoped_refptr<media::VideoFrame> txt_frame,
      scoped_refptr<media::VideoFrame> dest_frame,
      ReadbackDoneCallback done_cb);

  void OnARGBPixelsReadCompleted(scoped_refptr<media::VideoFrame> txt_frame,
                                 scoped_refptr<media::VideoFrame> dest_frame,
                                 ReadbackDoneCallback done_cb,
                                 bool success);

  void ReadbackYUVTextureBackedFrame(
      scoped_refptr<media::VideoFrame> txt_frame,
      scoped_refptr<media::VideoFrame> dest_frame,
      ReadbackDoneCallback done_cb);

  void OnYUVReadCompleted(scoped_refptr<media::VideoFrame> txt_frame,
                          scoped_refptr<media::VideoFrame> dest_frame,
                          scoped_refptr<media::VideoFrame> triplanar_frame,
                          ReadbackDoneCallback done_cb,
                          bool success);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Lives and dies on the worker thread.
  scoped_refptr<SyncReadbackThread> sync_readback_impl_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;
  media::VideoFramePool result_frame_pool_;
  media::VideoFrameConverter converter_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_BACKGROUND_READBACK_H_
