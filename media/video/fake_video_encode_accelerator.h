// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_FAKE_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_VIDEO_FAKE_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/video_encode_accelerator.h"

namespace base {

class SingleThreadTaskRunner;

}  // namespace base

namespace media {

static const size_t kMinimumOutputBufferSize = 123456;

class FakeVideoEncodeAccelerator : public VideoEncodeAccelerator {
 public:
  explicit FakeVideoEncodeAccelerator(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~FakeVideoEncodeAccelerator() override;

  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config, Client* client) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void RequestEncodingParametersChange(const VideoBitrateAllocation& bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;

  const std::vector<uint32_t>& stored_bitrates() const {
    return stored_bitrates_;
  }
  const std::vector<VideoBitrateAllocation>& stored_bitrate_allocations()
      const {
    return stored_bitrate_allocations_;
  }
  void SendDummyFrameForTesting(bool key_frame);
  void SetWillInitializationSucceed(bool will_initialization_succeed);

  size_t minimum_output_buffer_size() const { return kMinimumOutputBufferSize; }

 private:
  void DoRequireBitstreamBuffers(unsigned int input_count,
                                 const gfx::Size& input_coded_size,
                                 size_t output_buffer_size) const;
  void EncodeTask();
  void DoBitstreamBufferReady(int32_t bitstream_buffer_id,
                              size_t payload_size,
                              bool key_frame) const;

  // Our original (constructor) calling message loop used for all tasks.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::vector<uint32_t> stored_bitrates_;
  std::vector<VideoBitrateAllocation> stored_bitrate_allocations_;
  bool will_initialization_succeed_;

  VideoEncodeAccelerator::Client* client_;

  // Keeps track of if the current frame is the first encoded frame. This
  // is used to force a fake key frame for the first encoded frame.
  bool next_frame_is_first_frame_;

  // A queue containing the necessary data for incoming frames. The boolean
  // represent whether the queued frame should force a key frame.
  base::queue<bool> queued_frames_;

  // A list of buffers available for putting fake encoded frames in.
  std::list<BitstreamBuffer> available_buffers_;

  base::WeakPtrFactory<FakeVideoEncodeAccelerator> weak_this_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_VIDEO_FAKE_VIDEO_ENCODE_ACCELERATOR_H_
