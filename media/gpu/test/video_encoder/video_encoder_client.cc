// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/video_encoder_client.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// TODO(crbug.com/1045825): Support encoding parameter changes.

// Callbacks can be called from any thread, but WeakPtrs are not thread-safe.
// This helper thunk wraps a WeakPtr into an 'Optional' value, so the WeakPtr is
// only dereferenced after rescheduling the task on the specified task runner.
template <typename CallbackFunc, typename... CallbackArgs>
void CallbackThunk(
    base::Optional<base::WeakPtr<VideoEncoderClient>> encoder_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CallbackFunc func,
    CallbackArgs... args) {
  DCHECK(encoder_client);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(func, *encoder_client, args...));
}

std::vector<VideoEncodeAccelerator::Config::SpatialLayer>
CreateSpatialLayersConfig(const gfx::Size& resolution,
                          const VideoEncoderClientConfig& config) {
  // Returns empty spatial layer config because one temporal layer stream is
  // equivalent to a simple stream.
  if (config.num_temporal_layers == 1u)
    return {};

  // VideoEncodeAccelerator supports only temporal layer encoding.
  VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
  spatial_layer.width = resolution.width();
  spatial_layer.height = resolution.height();
  spatial_layer.bitrate_bps = config.bitrate;
  spatial_layer.framerate = config.framerate;
  spatial_layer.num_of_temporal_layers = config.num_temporal_layers;
  // Note: VideoEncodeAccelerator currently ignores this max_qp parameter.
  spatial_layer.max_qp = 30u;
  return {spatial_layer};
}
}  // namespace

VideoEncoderClientConfig::VideoEncoderClientConfig(
    const Video* video,
    VideoCodecProfile output_profile,
    size_t num_temporal_layers,
    uint32_t bitrate)
    : output_profile(output_profile),
      num_temporal_layers(num_temporal_layers),
      bitrate(bitrate),
      framerate(video->FrameRate()),
      num_frames_to_encode(video->NumFrames()) {}

VideoEncoderClientConfig::VideoEncoderClientConfig(
    const VideoEncoderClientConfig&) = default;

VideoEncoderStats::VideoEncoderStats() = default;
VideoEncoderStats::~VideoEncoderStats() = default;
VideoEncoderStats::VideoEncoderStats(const VideoEncoderStats&) = default;

VideoEncoderStats::VideoEncoderStats(uint32_t framerate,
                                     size_t num_temporal_layers)
    : framerate(framerate),
      num_encoded_frames_per_layer(num_temporal_layers, 0),
      encoded_frames_size_per_layer(num_temporal_layers, 0) {}

uint32_t VideoEncoderStats::Bitrate() const {
  auto compute_bitrate = [](double framerate, size_t num_frames,
                            size_t total_size,
                            base::Optional<size_t> layer_index) {
    const size_t average_frame_size_in_bits = total_size * 8 / num_frames;
    const uint32_t average_bitrate = average_frame_size_in_bits * framerate;
    const std::string prefix =
        layer_index ? "[TL#" + std::to_string(*layer_index) + "] " : "[Total] ";
    VLOGF(2) << prefix << "encoded_frames=" << num_frames
             << ", framerate=" << framerate
             << ", total_encoded_frames_size=" << total_size
             << ", average_frame_size_in_bits=" << average_frame_size_in_bits
             << ", average bitrate=" << average_bitrate;
    return average_bitrate;
  };

  const size_t num_layers = num_encoded_frames_per_layer.size();
  if (num_layers == 1) {
    return compute_bitrate(framerate, total_num_encoded_frames,
                           total_encoded_frames_size, base::nullopt);
  }

  for (size_t i = 0; i < num_layers; ++i) {
    // Used to compute the ratio of the framerate on each layer. For example,
    // when the number of temporal layers is three, the ratio of framerate of
    // layers are 1/4, 1/4 and 1/2 for the first, second and third layer,
    // respectively.
    constexpr size_t kFramerateDenom[][3] = {
        {1, 0, 0},
        {2, 2, 0},
        {4, 4, 2},
    };
    const size_t num_frames = num_encoded_frames_per_layer[i];
    const size_t frames_size = encoded_frames_size_per_layer[i];
    const uint32_t layer_framerate =
        static_cast<double>(framerate) / kFramerateDenom[num_layers - 1][i];
    compute_bitrate(layer_framerate, num_frames, frames_size, i);
  }
  return compute_bitrate(framerate, total_num_encoded_frames,
                         total_encoded_frames_size, base::nullopt);
}

void VideoEncoderStats::Reset() {
  total_num_encoded_frames = 0;
  total_encoded_frames_size = 0;
  std::fill(num_encoded_frames_per_layer.begin(),
            num_encoded_frames_per_layer.end(), 0u);
  std::fill(encoded_frames_size_per_layer.begin(),
            encoded_frames_size_per_layer.end(), 0u);
}

VideoEncoderClient::VideoEncoderClient(
    const VideoEncoder::EventCallback& event_cb,
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    const VideoEncoderClientConfig& config)
    : event_cb_(event_cb),
      bitstream_processors_(std::move(bitstream_processors)),
      encoder_client_config_(config),
      encoder_client_thread_("VDAClientEncoderThread"),
      encoder_client_state_(VideoEncoderClientState::kUninitialized),
      current_stats_(encoder_client_config_.framerate,
                     config.num_temporal_layers),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {
  DETACH_FROM_SEQUENCE(encoder_client_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VideoEncoderClient::~VideoEncoderClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  Destroy();
}

// static
std::unique_ptr<VideoEncoderClient> VideoEncoderClient::Create(
    const VideoEncoder::EventCallback& event_cb,
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors,
    gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory,
    const VideoEncoderClientConfig& config) {
  return base::WrapUnique(
      new VideoEncoderClient(event_cb, std::move(bitstream_processors),
                             gpu_memory_buffer_factory, config));
}

bool VideoEncoderClient::Initialize(const Video* video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);
  DCHECK(video);

  if (!encoder_client_thread_.Start()) {
    VLOGF(1) << "Failed to start encoder thread";
    return false;
  }
  encoder_client_task_runner_ = encoder_client_thread_.task_runner();

  bool success = false;
  base::WaitableEvent done;
  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::CreateEncoderTask,
                                weak_this_, video, &success, &done));
  done.Wait();

  return success;
}

void VideoEncoderClient::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  if (!encoder_client_thread_.IsRunning())
    return;

  base::WaitableEvent done;
  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::DestroyEncoderTask,
                                weak_this_, &done));
  done.Wait();

  // Wait until the bitstream processors are done before destroying them.
  // This needs to be done after destroying the encoder so no new bitstream
  // buffers will be queued while waiting.
  WaitForBitstreamProcessors();
  bitstream_processors_.clear();

  encoder_client_thread_.Stop();
}

void VideoEncoderClient::Encode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::EncodeTask, weak_this_));
}

void VideoEncoderClient::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::FlushTask, weak_this_));
}

void VideoEncoderClient::UpdateBitrate(const VideoBitrateAllocation& bitrate,
                                       uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::UpdateBitrateTask,
                                weak_this_, bitrate, framerate));
}

bool VideoEncoderClient::WaitForBitstreamProcessors() {
  bool success = true;
  for (auto& bitstream_processor : bitstream_processors_)
    success &= bitstream_processor->WaitUntilDone();
  return success;
}

VideoEncoderStats VideoEncoderClient::GetStats() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);
  base::AutoLock auto_lock(stats_lock_);
  return current_stats_;
}

void VideoEncoderClient::ResetStats() {
  base::AutoLock auto_lock(stats_lock_);
  current_stats_.Reset();
}

void VideoEncoderClient::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  ASSERT_EQ(encoder_client_state_, VideoEncoderClientState::kUninitialized);
  ASSERT_EQ(bitstream_buffers_.size(), 0u);
  ASSERT_GT(input_count, 0UL);
  ASSERT_GT(output_buffer_size, 0UL);
  DVLOGF(4);

  // TODO(crbug.com/1045825): Add support for videos with a visible rectangle
  // not starting at (0,0).
  aligned_data_helper_ = std::make_unique<AlignedDataHelper>(
      video_->Data(), video_->NumFrames(), video_->PixelFormat(),
      gfx::Rect(video_->Resolution()), input_coded_size,
      encoder_client_config_.input_storage_type ==
              VideoEncodeAccelerator::Config::StorageType::kDmabuf
          ? VideoFrame::STORAGE_GPU_MEMORY_BUFFER
          : VideoFrame::STORAGE_MOJO_SHARED_BUFFER,
      gpu_memory_buffer_factory_);

  output_buffer_size_ = output_buffer_size;

  for (unsigned int i = 0; i < input_count; ++i) {
    auto shm = base::UnsafeSharedMemoryRegion::Create(output_buffer_size_);
    LOG_ASSERT(shm.IsValid());

    BitstreamBuffer bitstream_buffer(GetNextBitstreamBufferId(),
                                     shm.Duplicate(), output_buffer_size_);

    bitstream_buffers_.insert(
        std::make_pair(bitstream_buffer.id(), std::move(shm)));

    encoder_->UseOutputBitstreamBuffer(std::move(bitstream_buffer));
  }

  // Notify the test video encoder that initialization is now complete.
  encoder_client_state_ = VideoEncoderClientState::kIdle;
  FireEvent(VideoEncoder::EncoderEvent::kInitialized);
}

scoped_refptr<BitstreamProcessor::BitstreamRef>
VideoEncoderClient::CreateBitstreamRef(
    int32_t bitstream_buffer_id,
    const BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  auto it = bitstream_buffers_.find(bitstream_buffer_id);
  LOG_ASSERT(it != bitstream_buffers_.end());
  auto decoder_buffer = DecoderBuffer::FromSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          it->second.Duplicate()),
      0u /* offset */, metadata.payload_size_bytes);
  if (!decoder_buffer)
    return nullptr;
  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(frame_index_));

  return BitstreamProcessor::BitstreamRef::Create(
      std::move(decoder_buffer), metadata, bitstream_buffer_id,
      BindToCurrentLoop(
          base::BindOnce(&VideoEncoderClient::BitstreamBufferProcessed,
                         weak_this_, bitstream_buffer_id)));
}

void VideoEncoderClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4) << "frame_index=" << frame_index_
            << ", encoded image size=" << metadata.payload_size_bytes;
  {
    base::AutoLock auto_lock(stats_lock_);
    current_stats_.total_num_encoded_frames++;
    current_stats_.total_encoded_frames_size += metadata.payload_size_bytes;
    if (metadata.vp9.has_value()) {
      uint8_t temporal_id = metadata.vp9->temporal_idx;
      ASSERT_LE(temporal_id,
                current_stats_.num_encoded_frames_per_layer.size());
      ASSERT_LE(temporal_id,
                current_stats_.encoded_frames_size_per_layer.size());
      current_stats_.num_encoded_frames_per_layer[temporal_id]++;
      current_stats_.encoded_frames_size_per_layer[temporal_id] +=
          metadata.payload_size_bytes;
    }
  }

  auto it = bitstream_buffers_.find(bitstream_buffer_id);
  ASSERT_NE(it, bitstream_buffers_.end());

  // Notify the test an encoded bitstream buffer is ready. We should only do
  // this after scheduling the bitstream to be processed, so calling
  // WaitForBitstreamProcessors() after receiving this event will always
  // guarantee the bitstream to be processed.
  FireEvent(VideoEncoder::EncoderEvent::kBitstreamReady);

  if (bitstream_processors_.empty()) {
    BitstreamBufferProcessed(bitstream_buffer_id);
    return;
  }

  auto bitstream_ref = CreateBitstreamRef(bitstream_buffer_id, metadata);
  ASSERT_TRUE(bitstream_ref);
  for (auto& bitstream_processor_ : bitstream_processors_) {
    bitstream_processor_->ProcessBitstream(bitstream_ref, frame_index_);
  }
  frame_index_++;
  FlushDoneTaskIfNeeded();
}

void VideoEncoderClient::FlushDoneTaskIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  // If the encoder does not support flushing, we have to manually call
  // FlushDoneTask(). Invoke FlushDoneTask() when
  // 1.) Flush is not supported by VideoEncodeAccelerator,
  // 2.) all the frames have been returned and
  // 3.) bitstreams of all the video frames have been output.
  // This is only valid if we always flush at the end of the stream (not in a
  // middle of the stream), which is the case in all of our test cases.
  if (!encoder_->IsFlushSupported() &&
      encoder_client_state_ == VideoEncoderClientState::kFlushing &&
      frame_index_ == encoder_client_config_.num_frames_to_encode &&
      num_outstanding_encode_requests_ == 0) {
    FlushDoneTask(true);
  }
}

void VideoEncoderClient::BitstreamBufferProcessed(int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  auto it = bitstream_buffers_.find(bitstream_buffer_id);
  ASSERT_NE(it, bitstream_buffers_.end());

  BitstreamBuffer bitstream_buffer(bitstream_buffer_id, it->second.Duplicate(),
                                   output_buffer_size_);
  encoder_->UseOutputBitstreamBuffer(std::move(bitstream_buffer));
}

void VideoEncoderClient::NotifyError(VideoEncodeAccelerator::Error error) {}

void VideoEncoderClient::NotifyEncoderInfoChange(const VideoEncoderInfo& info) {
}

void VideoEncoderClient::CreateEncoderTask(const Video* video,
                                           bool* success,
                                           base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_EQ(encoder_client_state_, VideoEncoderClientState::kUninitialized);
  ASSERT_TRUE(!encoder_) << "Can't create encoder: already created";
  ASSERT_TRUE(video);

  video_ = video;

  const VideoEncodeAccelerator::Config config(
      video_->PixelFormat(), video_->Resolution(),
      encoder_client_config_.output_profile, encoder_client_config_.bitrate,
      encoder_client_config_.framerate, base::nullopt /* gop_length */,
      base::nullopt /* h264_output_level*/, false /* is_constrained_h264 */,
      encoder_client_config_.input_storage_type,
      VideoEncodeAccelerator::Config::ContentType::kCamera,
      CreateSpatialLayersConfig(video_->Resolution(), encoder_client_config_));

  encoder_ = GpuVideoEncodeAcceleratorFactory::CreateVEA(
      config, this, gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds());
  *success = (encoder_ != nullptr);

  // Initialization is continued once the encoder notifies us of the coded size
  // in RequireBitstreamBuffers().
  done->Signal();
}

void VideoEncoderClient::DestroyEncoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_encode_requests_);
  DVLOGF(4);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  // Destroy the encoder. This will destroy all video frames.
  encoder_ = nullptr;

  encoder_client_state_ = VideoEncoderClientState::kUninitialized;
  done->Signal();
}

void VideoEncoderClient::EncodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  ASSERT_EQ(encoder_client_state_, VideoEncoderClientState::kIdle);
  DVLOGF(4);

  // Start encoding the first frames. While in the encoding state new frames
  // will automatically be fed to the encoder, when an input frame is returned
  // to us in EncodeDoneTask().
  encoder_client_state_ = VideoEncoderClientState::kEncoding;
  for (size_t i = 0; i < encoder_client_config_.max_outstanding_encode_requests;
       ++i) {
    EncodeNextFrameTask();
  }
}

void VideoEncoderClient::EncodeNextFrameTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4);
  // Stop encoding frames if we're no longer in the encoding state.
  if (encoder_client_state_ != VideoEncoderClientState::kEncoding)
    return;

  const bool end_of_stream =
      encoder_client_config_.num_frames_to_encode == num_encodes_requested_;
  if (end_of_stream) {
    // Flush immediately when we reached the end of the stream (either the real
    // end, or the artificial end when using num_encode_frames). This changes
    // the state to kFlushing so further encode tasks will be aborted.
    FlushTask();
    return;
  }
  if (aligned_data_helper_->AtEndOfStream())
    aligned_data_helper_->Rewind();

  scoped_refptr<VideoFrame> video_frame = aligned_data_helper_->GetNextFrame();
  ASSERT_TRUE(video_frame);
  video_frame->AddDestructionObserver(base::BindOnce(
      CallbackThunk<decltype(&VideoEncoderClient::EncodeDoneTask),
                    base::TimeDelta>,
      weak_this_, encoder_client_task_runner_,
      &VideoEncoderClient::EncodeDoneTask, video_frame->timestamp()));

  // TODO(dstaessens): Add support for forcing key frames.
  bool force_keyframe = false;
  encoder_->Encode(video_frame, force_keyframe);

  num_encodes_requested_++;
  num_outstanding_encode_requests_++;
}

void VideoEncoderClient::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4);
  // Changing the state to flushing will abort any pending encodes.
  encoder_client_state_ = VideoEncoderClientState::kFlushing;

  if (!encoder_->IsFlushSupported()) {
    FireEvent(VideoEncoder::EncoderEvent::kFlushing);
    FlushDoneTaskIfNeeded();
    return;
  }

  auto flush_done_cb = base::BindOnce(
      CallbackThunk<decltype(&VideoEncoderClient::FlushDoneTask), bool>,
      weak_this_, encoder_client_task_runner_,
      &VideoEncoderClient::FlushDoneTask);
  encoder_->Flush(std::move(flush_done_cb));

  FireEvent(VideoEncoder::EncoderEvent::kFlushing);
}

void VideoEncoderClient::UpdateBitrateTask(
    const VideoBitrateAllocation& bitrate,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4);
  encoder_->RequestEncodingParametersChange(bitrate, framerate);
  base::AutoLock auto_lcok(stats_lock_);
  current_stats_.framerate = framerate;
}

void VideoEncoderClient::EncodeDoneTask(base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_NE(VideoEncoderClientState::kIdle, encoder_client_state_);
  DVLOGF(4);

  FireEvent(VideoEncoder::EncoderEvent::kFrameReleased);

  num_outstanding_encode_requests_--;
  FlushDoneTaskIfNeeded();

  // Queue the next frame to be encoded.
  encoder_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoderClient::EncodeNextFrameTask, weak_this_));
}

void VideoEncoderClient::FlushDoneTask(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_encode_requests_);
  ASSERT_TRUE(success) << "Failed to flush encoder";

  encoder_client_state_ = VideoEncoderClientState::kIdle;
  FireEvent(VideoEncoder::EncoderEvent::kFlushDone);
}

void VideoEncoderClient::FireEvent(VideoEncoder::EncoderEvent event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);

  bool continue_encoding = event_cb_.Run(event);
  if (!continue_encoding) {
    // Changing the state to idle will abort any pending encodes.
    encoder_client_state_ = VideoEncoderClientState::kIdle;
  }
}

int32_t VideoEncoderClient::GetNextBitstreamBufferId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  // The bitstream buffer ID should always be positive, negative values are
  // reserved for uninitialized buffers.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x7FFFFFFF;
  return next_bitstream_buffer_id_;
}

}  // namespace test
}  // namespace media
