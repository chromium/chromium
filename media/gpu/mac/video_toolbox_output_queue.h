// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_OUTPUT_QUEUE_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_OUTPUT_QUEUE_H_

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Handles frame reordering for VideoToolboxVideoDecoder.
class MEDIA_GPU_EXPORT VideoToolboxOutputQueue {
 public:
  // Callbacks are posted to `task_runner`.
  explicit VideoToolboxOutputQueue(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~VideoToolboxOutputQueue();

  // Must be called at least once before fulfilling any picture.
  void SetOutputCB(const VideoDecoder::OutputCB output_cb);

  // Schedule `picture` next in presentation order.
  void SchedulePicture(scoped_refptr<CodecPicture> picture);

  // Provides `frame` to be output for `picture`. Pictures can be fulfilled
  // before or after they are scheduled.
  void FulfillPicture(scoped_refptr<CodecPicture> picture,
                      scoped_refptr<VideoFrame> frame);

  // Request `flush_cb` be called when there are no more scheduled pictures.
  void Flush(VideoDecoder::DecodeCB flush_cb);

  // Discard all state. If there is a pending flush, it is called with |status|.
  void Reset(DecoderStatus status);

 private:
  void Process();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  VideoDecoder::OutputCB output_cb_;
  VideoDecoder::DecodeCB flush_cb_;

  base::queue<scoped_refptr<CodecPicture>> scheduled_pictures_;
  base::flat_map<scoped_refptr<CodecPicture>, scoped_refptr<VideoFrame>>
      fulfilled_pictures_;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_OUTPUT_QUEUE_H_
