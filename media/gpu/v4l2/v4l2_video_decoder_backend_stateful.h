// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATEFUL_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATEFUL_H_

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "media/base/video_codecs.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend.h"

namespace media {

namespace v4l2_vda_helpers {
class InputBufferFragmentSplitter;
}

class V4L2StatefulVideoDecoderBackend : public V4L2VideoDecoderBackend {
 public:
  V4L2StatefulVideoDecoderBackend(
      Client* const client,
      scoped_refptr<V4L2Device> device,
      VideoCodecProfile profile,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~V4L2StatefulVideoDecoderBackend() override;

  // We don't ever want to copy or move this.
  V4L2StatefulVideoDecoderBackend(const V4L2StatefulVideoDecoderBackend&) =
      delete;
  V4L2StatefulVideoDecoderBackend& operator=(
      const V4L2StatefulVideoDecoderBackend&) = delete;

  // V4L2VideoDecoderBackend implementation
  bool Initialize() override;
  void EnqueueDecodeTask(scoped_refptr<DecoderBuffer> buffer,
                         VideoDecoder::DecodeCB decode_cb,
                         int32_t bitstream_id) override;
  void OnOutputBufferDequeued(V4L2ReadableBufferRef buffer) override;
  void OnServiceDeviceTask(bool event) override;
  void OnStreamStopped(bool stop_input_queue) override;
  bool ApplyResolution(const gfx::Size& pic_size,
                       const gfx::Rect& visible_rect,
                       const size_t num_output_frames) override;
  void OnChangeResolutionDone(bool success) override;
  void ClearPendingRequests(DecodeStatus status) override;
  bool StopInputQueueOnResChange() const override;

 private:
  // TODO(b:149663704): merge with stateless?
  // Request for decoding buffer. Every EnqueueDecodeTask() call generates 1
  // DecodeRequest.
  struct DecodeRequest {
    // The decode buffer passed to EnqueueDecodeTask().
    scoped_refptr<DecoderBuffer> buffer;
    // Number of bytes used so far from |buffer|.
    size_t bytes_used = 0;
    // The callback function passed to EnqueueDecodeTask().
    VideoDecoder::DecodeCB decode_cb;
    // Identifier for the decoder buffer.
    int32_t bitstream_id;

    DecodeRequest(scoped_refptr<DecoderBuffer> buf,
                  VideoDecoder::DecodeCB cb,
                  int32_t id);

    // Allow move, but not copy
    DecodeRequest(DecodeRequest&&);
    DecodeRequest& operator=(DecodeRequest&&);

    ~DecodeRequest();

    bool IsCompleted() const;

    DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
  };

  bool IsSupportedProfile(VideoCodecProfile profile);

  void DoDecodeWork();

  static void ReuseOutputBufferThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Optional<base::WeakPtr<V4L2StatefulVideoDecoderBackend>> weak_this,
      V4L2ReadableBufferRef buffer);
  void ReuseOutputBuffer(V4L2ReadableBufferRef buffer);

  // Called when the format has changed, in order to reallocate the output
  // buffers according to the new format.
  void ChangeResolution();
  // Called when the flush triggered by a resolution change has completed,
  // to actually apply the resolution.
  void ContinueChangeResolution(const gfx::Size& pic_size,
                                const gfx::Rect& visible_rect,
                                const size_t num_output_buffers);

  // Enqueue all output buffers that are available.
  void EnqueueOutputBuffers();
  // When a video frame pool is in use, obtain a frame from the pool or, if
  // none is available, schedule |EnqueueOutputBuffers()| to be called when one
  // becomes available.
  scoped_refptr<VideoFrame> GetPoolVideoFrame();

  bool InitiateFlush(VideoDecoder::DecodeCB flush_cb);
  bool CompleteFlush();

  void ScheduleDecodeWork();

  // Process all the event in the event queue
  void ProcessEventQueue();

  // Video profile we are decoding.
  VideoCodecProfile profile_;

  // The task runner we are running on, for convenience.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // VideoCodecProfiles supported by a v4l2 stateless decoder driver.
  std::vector<VideoCodecProfile> supported_profiles_;

  // Queue of pending decode request.
  base::queue<DecodeRequest> decode_request_queue_;

  // The decode request which is currently processed.
  base::Optional<DecodeRequest> current_decode_request_;
  // V4L2 input buffer currently being prepared.
  base::Optional<V4L2WritableBufferRef> current_input_buffer_;

  std::unique_ptr<v4l2_vda_helpers::InputBufferFragmentSplitter>
      frame_splitter_;

  base::Optional<gfx::Rect> visible_rect_;

  // Callback of the buffer that triggered a flush, to be called when the
  // flush completes.
  VideoDecoder::DecodeCB flush_cb_;
  // Closure that will be called once the flush triggered by a resolution change
  // event completes.
  base::OnceClosure resolution_change_cb_;

  base::WeakPtr<V4L2StatefulVideoDecoderBackend> weak_this_;
  base::WeakPtrFactory<V4L2StatefulVideoDecoderBackend> weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATEFUL_H_
