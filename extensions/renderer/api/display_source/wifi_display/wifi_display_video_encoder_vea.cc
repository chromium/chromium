// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "content/public/renderer/video_encode_accelerator.h"
#include "media/base/bind_to_current_loop.h"

namespace extensions {

namespace {

// This video encoder implements hardware accelerated H.264 video encoding
// using media::VideoEncodeAccelerator.
class WiFiDisplayVideoEncoderVEA final
    : public WiFiDisplayVideoEncoder,
      public media::VideoEncodeAccelerator::Client {
 public:
  static void Create(
      const InitParameters& params,
      const VideoEncoderCallback& encoder_callback,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      std::unique_ptr<media::VideoEncodeAccelerator> vea);

 private:
  WiFiDisplayVideoEncoderVEA(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      media::VideoEncodeAccelerator* vea,
      const CreateEncodeMemoryCallback& create_memory_callback);
  ~WiFiDisplayVideoEncoderVEA() override;

  // media::VideoEncodeAccelerator::Client
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;

  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame,
                            base::TimeDelta timestamp) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

  scoped_refptr<WiFiDisplayVideoEncoder> InitOnMediaThread(
      const InitParameters& params);
  void InsertFrameOnMediaThread(scoped_refptr<media::VideoFrame> video_frame,
                                base::TimeTicks reference_time,
                                bool send_idr) override;
  void OnCreateSharedMemory(base::UnsafeSharedMemoryRegion memory);
  void OnReceivedSharedMemory(base::UnsafeSharedMemoryRegion memory);

 private:
  struct InProgressFrameEncode {
    InProgressFrameEncode(scoped_refptr<media::VideoFrame> video_frame,
                          base::TimeTicks reference_time);
    ~InProgressFrameEncode();
    // The source content to encode.
    const scoped_refptr<media::VideoFrame> video_frame;

    // The reference time for this frame.
    const base::TimeTicks reference_time;
  };
  // FIFO list.
  std::list<InProgressFrameEncode> in_progress_frame_encodes_;
  media::VideoEncodeAccelerator* vea_;  // Owned on media thread.
  std::vector<std::pair<base::UnsafeSharedMemoryRegion,
                        base::WritableSharedMemoryMapping>>
      output_buffers_;
  size_t output_buffers_count_;
  CreateEncodeMemoryCallback create_video_encode_memory_cb_;
};

WiFiDisplayVideoEncoderVEA::InProgressFrameEncode::InProgressFrameEncode(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time)
    : video_frame(std::move(video_frame)), reference_time(reference_time) {}

WiFiDisplayVideoEncoderVEA::InProgressFrameEncode::~InProgressFrameEncode() =
    default;

// static
void WiFiDisplayVideoEncoderVEA::Create(
    const InitParameters& params,
    const VideoEncoderCallback& encoder_callback,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    std::unique_ptr<media::VideoEncodeAccelerator> vea) {
  if (!vea || !media_task_runner) {
    encoder_callback.Run(nullptr);
    return;
  }

  base::PostTaskAndReplyWithResult(
      media_task_runner.get(), FROM_HERE,
      base::Bind(&WiFiDisplayVideoEncoderVEA::InitOnMediaThread,
                 base::WrapRefCounted(new WiFiDisplayVideoEncoderVEA(
                     std::move(media_task_runner), vea.release(),
                     params.create_memory_callback)),
                 params),
      encoder_callback);
}

WiFiDisplayVideoEncoderVEA::WiFiDisplayVideoEncoderVEA(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    media::VideoEncodeAccelerator* vea,
    const CreateEncodeMemoryCallback& create_video_encode_memory_cb)
    : WiFiDisplayVideoEncoder(std::move(media_task_runner)),
      vea_(vea),
      output_buffers_count_(0),
      create_video_encode_memory_cb_(create_video_encode_memory_cb) {}

WiFiDisplayVideoEncoderVEA::~WiFiDisplayVideoEncoderVEA() {
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&media::VideoEncodeAccelerator::Destroy,
                                base::Unretained(vea_)));
}

scoped_refptr<WiFiDisplayVideoEncoder>
WiFiDisplayVideoEncoderVEA::InitOnMediaThread(const InitParameters& params) {
  media::VideoCodecProfile profile = (params.profile == wds::CHP)
      ? media::H264PROFILE_HIGH : media::H264PROFILE_BASELINE;
  const media::VideoEncodeAccelerator::Config config(
      media::PIXEL_FORMAT_I420, params.frame_size, profile, params.bit_rate);
  bool success = vea_->Initialize(config, this);
  if (success)
    return this;

  DVLOG(1) << "Failed to init VEA";
  return nullptr;
}

// Called to allocate the input and output buffers.
void WiFiDisplayVideoEncoderVEA::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  output_buffers_count_ = input_count;
  for (size_t i = 0; i < input_count; ++i) {
    create_video_encode_memory_cb_.Run(
        output_buffer_size,
        base::Bind(&WiFiDisplayVideoEncoderVEA::OnCreateSharedMemory, this));
  }
}

// Note: This method can be called on any thread.
void WiFiDisplayVideoEncoderVEA::OnCreateSharedMemory(
    base::UnsafeSharedMemoryRegion memory) {
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WiFiDisplayVideoEncoderVEA::OnReceivedSharedMemory, this,
                     std::move(memory)));
}

void WiFiDisplayVideoEncoderVEA::OnReceivedSharedMemory(
    base::UnsafeSharedMemoryRegion memory) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  base::WritableSharedMemoryMapping mapping = memory.Map();
  output_buffers_.push_back(
      std::make_pair(std::move(memory), std::move(mapping)));

  // Wait until all requested buffers are received.
  if (output_buffers_.size() < output_buffers_count_)
    return;

  // Immediately provide all output buffers to the VEA.
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    vea_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        static_cast<int32_t>(i), output_buffers_[i].first.Duplicate(),
        output_buffers_[i].first.GetSize()));
  }
}

void WiFiDisplayVideoEncoderVEA::InsertFrameOnMediaThread(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time,
    bool send_idr) {
  in_progress_frame_encodes_.emplace_back(video_frame, reference_time);
  DCHECK(vea_);
  vea_->Encode(video_frame, send_idr);
}

void WiFiDisplayVideoEncoderVEA::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    size_t payload_size,
    bool key_frame,
    base::TimeDelta timestamp) {
  if (bitstream_buffer_id >= static_cast<int>(output_buffers_.size())) {
    DVLOG(1) << "WiFiDisplayVideoEncoderVEA::BitstreamBufferReady()"
             << ": invalid bitstream_buffer_id=" << bitstream_buffer_id;
    return;
  }
  const base::WritableSharedMemoryMapping& output_buffer =
      output_buffers_[bitstream_buffer_id].second;
  if (payload_size > output_buffer.size()) {
    DVLOG(1) << "WiFiDisplayVideoEncoderVEA::BitstreamBufferReady()"
             << ": invalid payload_size=" << payload_size;
    return;
  }
  if (in_progress_frame_encodes_.empty()) {
    DVLOG(1) << "WiFiDisplayVideoEncoderVEA::BitstreamBufferReady()"
             << ": unexpected frame";
    return;
  }
  const InProgressFrameEncode& request = in_progress_frame_encodes_.front();

  if (!encoded_callback_.is_null()) {
    encoded_callback_.Run(
        std::unique_ptr<WiFiDisplayEncodedFrame>(new WiFiDisplayEncodedFrame(
            std::string(output_buffer.GetMemoryAsSpan<const char>().data(),
                        payload_size),
            request.reference_time, base::TimeTicks::Now(), key_frame)));
  }
  DCHECK(vea_);
  vea_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_id,
      output_buffers_[bitstream_buffer_id].first.Duplicate(),
      output_buffers_[bitstream_buffer_id].first.GetSize()));

  in_progress_frame_encodes_.pop_front();
}

void WiFiDisplayVideoEncoderVEA::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  if (!error_callback_.is_null())
    error_callback_.Run();
}

}  // namespace

// static
void WiFiDisplayVideoEncoder::CreateVEA(
    const InitParameters& params,
    const VideoEncoderCallback& encoder_callback) {
  auto on_vea_cb =
      base::Bind(&WiFiDisplayVideoEncoderVEA::Create, params, encoder_callback);
  params.vea_create_callback.Run(media::BindToCurrentLoop(on_vea_cb));
}

}  // namespace extensions
