// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_LEGACY_V4L2_VIDEO_DECODER_BACKEND_STATEFUL_H_
#define MEDIA_GPU_V4L2_LEGACY_V4L2_VIDEO_DECODER_BACKEND_STATEFUL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_framerate_control.h"
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
      const VideoColorSpace& color_space,
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
                         VideoDecoder::DecodeCB decode_cb) override;
  void OnOutputBufferDequeued(V4L2ReadableBufferRef buffer) override;
  void OnServiceDeviceTask(bool event) override;
  void OnStreamStopped(bool stop_input_queue) override;
  bool ApplyResolution(const gfx::Size& pic_size,
                       const gfx::Rect& visible_rect) override;
  void OnChangeResolutionDone(CroStatus status) override;
  void ClearPendingRequests(DecoderStatus status) override;
  bool StopInputQueueOnResChange() const override;
  size_t GetNumOUTPUTQueueBuffers(bool secure_mode) const override;

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

    DecodeRequest(scoped_refptr<DecoderBuffer> buf, VideoDecoder::DecodeCB cb);

    DecodeRequest(const DecodeRequest&) = delete;
    DecodeRequest& operator=(const DecodeRequest&) = delete;

    // Allow move, but not copy
    DecodeRequest(DecodeRequest&&);
    DecodeRequest& operator=(DecodeRequest&&);

    ~DecodeRequest();

    bool IsCompleted() const;
  };

  bool IsSupportedProfile(VideoCodecProfile profile);

  void DoDecodeWork();

  static void ReuseOutputBufferThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::optional<base::WeakPtr<V4L2StatefulVideoDecoderBackend>> weak_this,
      V4L2ReadableBufferRef buffer);
  void ReuseOutputBuffer(V4L2ReadableBufferRef buffer);

  // Called when the format has changed, in order to reallocate the output
  // buffers according to the new format.
  void ChangeResolution();
  // Called when the flush triggered by a resolution change has completed,
  // to actually apply the resolution.
  void ContinueChangeResolution(const gfx::Size& pic_size,
                                const gfx::Rect& visible_rect,
                                const size_t num_codec_reference_frames,
                                uint8_t bit_depth);

  // Enqueue all output buffers that are available.
  void EnqueueOutputBuffers();
  // When a video frame pool is in use, obtain a frame from the pool or, if
  // none is available, schedule |EnqueueOutputBuffers()| to be called when one
  // becomes available.
  scoped_refptr<FrameResource> GetPoolVideoFrame();

  bool SendStopCommand();
  bool InitiateFlush(VideoDecoder::DecodeCB flush_cb);
  bool CompleteFlush();

  void ScheduleDecodeWork();

  // Process all the event in the event queue
  void ProcessEventQueue();

  // The name of the running driver.
  const std::string driver_name_;

  // Video profile we are decoding.
  VideoCodecProfile profile_;

  // Video color space we are decoding.
  VideoColorSpace color_space_;

  // The task runner we are running on, for convenience.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // VideoCodecProfiles supported by a v4l2 stateless decoder driver.
  std::vector<VideoCodecProfile> supported_profiles_;

  // Queue of pending decode request.
  base::queue<DecodeRequest> decode_request_queue_;

  // The decode request which is currently processed.
  std::optional<DecodeRequest> current_decode_request_;
  // V4L2 input buffer currently being prepared.
  std::optional<V4L2WritableBufferRef> current_input_buffer_;

  std::unique_ptr<v4l2_vda_helpers::InputBufferFragmentSplitter>
      frame_splitter_;

  std::optional<gfx::Rect> visible_rect_;

  // Map of enqueuing timecodes to system timestamp, for histogramming purposes.
  std::map<int64_t, base::TimeTicks> encoding_timestamps_;

  // Callback of the buffer that triggered a flush, to be called when the
  // flush completes.
  VideoDecoder::DecodeCB flush_cb_;
  // Closure that will be called once the flush triggered by a resolution change
  // event completes.
  base::OnceClosure resolution_change_cb_;

  // Whether there is any decoding request coming after
  // initialization/flush/reset is finished.
  // This flag is set on the first decode request, and reset after a successful
  // flush or reset.
  bool has_pending_requests_ = false;

  // The venus driver is the only implementation that requires the client
  // to inform the driver of the framerate.
  std::unique_ptr<V4L2FrameRateControl> framerate_control_;

  // If the resolution change is interrupted and aborted by reset, then V4L2
  // stateful API won't send the resolution change event again when the decoder
  // receives the input buffer with the same resolution after reset.
  // Set |need_resume_resolution_change_| to true in this scenario to resume the
  // resolution change after the reset is done.
  bool need_resume_resolution_change_ = false;

  base::WeakPtr<V4L2StatefulVideoDecoderBackend> weak_this_;
  base::WeakPtrFactory<V4L2StatefulVideoDecoderBackend> weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_LEGACY_V4L2_VIDEO_DECODER_BACKEND_STATEFUL_H_
