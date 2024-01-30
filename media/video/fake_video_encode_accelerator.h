// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_FAKE_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_VIDEO_FAKE_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/video_encode_accelerator.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace media {

static const size_t kMinimumOutputBufferSize = 123456;

class FakeVideoEncodeAccelerator : public VideoEncodeAccelerator {
 public:
  explicit FakeVideoEncodeAccelerator(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  FakeVideoEncodeAccelerator(const FakeVideoEncodeAccelerator&) = delete;
  FakeVideoEncodeAccelerator& operator=(const FakeVideoEncodeAccelerator&) =
      delete;

  ~FakeVideoEncodeAccelerator() override;

  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config,
                  Client* client,
                  std::unique_ptr<MediaLog> media_log = nullptr) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  bool IsGpuFrameResizeSupported() override;
  void Destroy() override;

  const std::vector<Bitrate>& stored_bitrates() const {
    return stored_bitrates_;
  }
  const std::vector<gfx::Size>& stored_frame_sizes() const {
    return stored_frame_sizes_;
  }
  const std::vector<VideoBitrateAllocation>& stored_bitrate_allocations()
      const {
    return stored_bitrate_allocations_;
  }
  void SetWillInitializationSucceed(bool will_initialization_succeed);
  void SetWillEncodingSucceed(bool will_encoding_succeed);
  void SetSupportFrameSizeChange(bool support_frame_size_change);

  size_t minimum_output_buffer_size() const { return kMinimumOutputBufferSize; }

  struct FrameToEncode {
    FrameToEncode();
    FrameToEncode(const FrameToEncode&);
    ~FrameToEncode();
    scoped_refptr<VideoFrame> frame;
    bool force_keyframe;
  };

  using EncodingCallback = base::RepeatingCallback<BitstreamBufferMetadata(
      BitstreamBuffer&,
      bool keyframe,
      scoped_refptr<VideoFrame> frame)>;

  void SetEncodingCallback(EncodingCallback callback) {
    encoding_callback_ = std::move(callback);
  }

  void SupportResize() { resize_supported_ = true; }

  void NotifyEncoderInfoChange(const VideoEncoderInfo& info);

 private:
  void DoNotifyEncoderInfoChange(const VideoEncoderInfo& info);
  void DoRequireBitstreamBuffers(unsigned int input_count,
                                 const gfx::Size& input_coded_size,
                                 size_t output_buffer_size) const;
  void EncodeTask();
  void DoBitstreamBufferReady(BitstreamBuffer buffer,
                              FrameToEncode frame_to_encode) const;
  void UpdateOutputFrameSize(const gfx::Size& size);

  // Our original (constructor) calling message loop used for all tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::vector<Bitrate> stored_bitrates_;
  std::vector<VideoBitrateAllocation> stored_bitrate_allocations_;
  std::vector<gfx::Size> stored_frame_sizes_;
  bool will_initialization_succeed_;
  bool will_encoding_succeed_;
  bool resize_supported_ = false;

  raw_ptr<VideoEncodeAccelerator::Client> client_;

  // Keeps track of if the current frame is the first encoded frame. This
  // is used to force a fake key frame for the first encoded frame.
  bool next_frame_is_first_frame_;

  // A queue containing the necessary data for incoming frames.
  base::queue<FrameToEncode> queued_frames_;

  // A list of buffers available for putting fake encoded frames in.
  std::list<BitstreamBuffer> available_buffers_;

  // Callback that, if set, does actual frame to buffer conversion.
  EncodingCallback encoding_callback_;

  // Current encoder info. Call |NotifyEncoderInfoChange| when it changes.
  VideoEncoderInfo encoder_info_;

  base::WeakPtrFactory<FakeVideoEncodeAccelerator> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_VIDEO_FAKE_VIDEO_ENCODE_ACCELERATOR_H_
