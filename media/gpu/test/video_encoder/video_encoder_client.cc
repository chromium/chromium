// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/video_encoder/video_encoder_client.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/bitrate.h"
#include "media/base/media_log.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/gpu/test/raw_video.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Minimum number of bitstream buffers we need to make sure we don't risk a
// deadlock. See crrev/c/2340653.
// FFmpeg decoder buffers until its thread pool is full. The number of desired
// threads is 12 in 4k.
// https://source.chromium.org/chromium/chromium/src/+/main:media/filters/ffmpeg_video_decoder.cc;l=94;drc=002c0bc1ac64f33a327a42a54afb87500943a3b3
// Therefore, we need to have the number of bitstream buffers. See b/277368164.
static unsigned int kMinInFlightFrames = 12;

// TODO(crbug.com/1045825): Support encoding parameter changes.

// Callbacks can be called from any thread, but WeakPtrs are not thread-safe.
// This helper thunk wraps a WeakPtr into an 'Optional' value, so the WeakPtr is
// only dereferenced after rescheduling the task on the specified task runner.
template <typename CallbackFunc, typename... CallbackArgs>
void CallbackThunk(
    std::optional<base::WeakPtr<VideoEncoderClient>> encoder_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CallbackFunc func,
    CallbackArgs... args) {
  DCHECK(encoder_client);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(func, *encoder_client, args...));
}

}  // namespace

VideoEncoderClientConfig::VideoEncoderClientConfig(
    const RawVideo* video,
    VideoCodecProfile output_profile,
    const std::vector<VideoEncodeAccelerator::Config::SpatialLayer>&
        spatial_layers,
    SVCInterLayerPredMode inter_layer_pred_mode,
    VideoEncodeAccelerator::Config::ContentType content_type,
    const VideoBitrateAllocation& bitrate_allocation,
    bool reverse)
    : output_profile(output_profile),
      output_resolution(video->Resolution()),
      spatial_layers(spatial_layers),
      num_temporal_layers(spatial_layers.empty()
                              ? 1
                              : spatial_layers[0].num_of_temporal_layers),
      num_spatial_layers(
          std::max(spatial_layers.size(), static_cast<size_t>(1u))),
      inter_layer_pred_mode(inter_layer_pred_mode),
      content_type(content_type),
      bitrate_allocation(bitrate_allocation),
      framerate(video->FrameRate()),
      num_frames_to_encode(video->NumFrames()),
      reverse(reverse) {
  CHECK(inter_layer_pred_mode == SVCInterLayerPredMode::kOff ||
        inter_layer_pred_mode == SVCInterLayerPredMode::kOnKeyPic);
}

VideoEncoderClientConfig::VideoEncoderClientConfig(
    const VideoEncoderClientConfig&) = default;
VideoEncoderClientConfig::~VideoEncoderClientConfig() = default;

VideoEncoderStats::VideoEncoderStats() = default;
VideoEncoderStats::~VideoEncoderStats() = default;
VideoEncoderStats::VideoEncoderStats(const VideoEncoderStats&) = default;

VideoEncoderStats::VideoEncoderStats(uint32_t framerate,
                                     size_t num_temporal_layers,
                                     size_t num_spatial_layers)
    : framerate(framerate),
      num_encoded_frames_per_layer(num_spatial_layers,
                                   std::vector<size_t>(num_temporal_layers, 0)),
      encoded_frames_size_per_layer(
          num_spatial_layers,
          std::vector<size_t>(num_temporal_layers, 0)),
      num_spatial_layers(num_spatial_layers),
      num_temporal_layers(num_temporal_layers) {}

uint32_t VideoEncoderStats::Bitrate() const {
  const size_t average_frame_size_in_bits =
      total_encoded_frames_size * 8 / total_num_encoded_frames;
  const uint32_t average_bitrate = base::checked_cast<uint32_t>(
      average_frame_size_in_bits * framerate * num_spatial_layers);
  VLOGF(2) << " [Total] encoded_frames=" << total_num_encoded_frames
           << ", framerate=" << framerate
           << ", num_spatial_layers=" << num_spatial_layers
           << ", total_encoded_frames_size=" << total_encoded_frames_size
           << ", average_frame_size_in_bits=" << average_frame_size_in_bits
           << ", average bitrate=" << average_bitrate;

  return average_bitrate;
}

uint32_t VideoEncoderStats::LayerBitrate(size_t spatial_idx,
                                         size_t temporal_idx) const {
  const size_t num_frames =
      num_encoded_frames_per_layer[spatial_idx][temporal_idx];
  const size_t frames_size =
      encoded_frames_size_per_layer[spatial_idx][temporal_idx];
  // Used to compute the ratio of the framerate on each layer. For example,
  // when the number of temporal layers is three, the ratio of framerate of
  // layers are 1/4, 1/4 and 1/2 for the first, second and third layer,
  // respectively.
  constexpr size_t kFramerateDenom[][3] = {
      {1, 0, 0},
      {2, 2, 0},
      {4, 4, 2},
  };

  const double layer_framerate =
      static_cast<double>(framerate) /
      kFramerateDenom[num_temporal_layers - 1][temporal_idx];
  const double average_frame_size_in_bits = frames_size * 8 / num_frames;
  const uint32_t average_bitrate = base::checked_cast<uint32_t>(
      average_frame_size_in_bits * layer_framerate);

  std::string prefix;
  if (num_spatial_layers > 1) {
    prefix = "[SL#" + base::NumberToString(spatial_idx) + " TL#" +
             base::NumberToString(temporal_idx) + "] ";
  } else {
    DCHECK_NE(num_temporal_layers, 1u);
    prefix = "[TL#" + base::NumberToString(temporal_idx) + "] ";
  }

  VLOGF(2) << prefix << "encoded_frames=" << num_frames
           << ", framerate=" << layer_framerate
           << ", total_encoded_frames_size=" << frames_size
           << ", average_frame_size_in_bits=" << average_frame_size_in_bits
           << ", average bitrate=" << average_bitrate;

  return average_bitrate;
}

void VideoEncoderStats::Reset() {
  total_num_encoded_frames = 0;
  total_encoded_frames_size = 0;
  std::fill(num_encoded_frames_per_layer.begin(),
            num_encoded_frames_per_layer.end(),
            std::vector<size_t>(num_temporal_layers, 0u));
  std::fill(encoded_frames_size_per_layer.begin(),
            encoded_frames_size_per_layer.end(),
            std::vector<size_t>(num_temporal_layers, 0u));
}

VideoEncoderClient::VideoEncoderClient(
    const VideoEncoder::EventCallback& event_cb,
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors,
    const VideoEncoderClientConfig& config)
    : event_cb_(event_cb),
      bitstream_processors_(std::move(bitstream_processors)),
      encoder_client_config_(config),
      encoder_client_thread_("VDAClientEncoderThread"),
      encoder_client_state_(VideoEncoderClientState::kUninitialized),
      current_stats_(encoder_client_config_.framerate,
                     config.num_temporal_layers,
                     config.num_spatial_layers) {
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
    const VideoEncoderClientConfig& config) {
  return base::WrapUnique(new VideoEncoderClient(
      event_cb, std::move(bitstream_processors), config));
}

bool VideoEncoderClient::Initialize(const RawVideo* video) {
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

void VideoEncoderClient::ForceKeyFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  encoder_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoderClient::ForceKeyFrameTask, weak_this_));
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

  input_count = std::max(kMinInFlightFrames, input_count);

  gfx::Size coded_size = input_coded_size;
  if (video_->Resolution() != encoder_client_config_.output_resolution) {
    // Scaling case. Scaling is currently only supported when using Dmabufs.
    EXPECT_EQ(encoder_client_config_.input_storage_type,
              VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer);
    coded_size = video_->Resolution();
  }

  // Timestamps are applied to the frames before they are submitted to the
  // encoder.  If encode is to run as fast as possible, then the
  // timestamps need to be spaced according to the framerate.
  // If the encoder is to encode real-time, then |encode_interval|
  // will be used to only submit frames every |encode_interval|.
  const uint32_t frame_rate =
      encoder_client_config_.encode_interval ? 0 : video_->FrameRate();

  // Follow the behavior of the chrome capture stack; |natural_size| is the
  // dimension to be encoded.
  aligned_data_helper_ = std::make_unique<AlignedDataHelper>(
      video_, encoder_client_config_.num_frames_to_encode,
      encoder_client_config_.reverse,
      /*dst_coded_size=*/coded_size,
      /*natural_size=*/encoder_client_config_.output_resolution, frame_rate,
      encoder_client_config_.input_storage_type ==
              VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer
          ? VideoFrame::STORAGE_GPU_MEMORY_BUFFER
          : VideoFrame::STORAGE_SHMEM);

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

  scoped_refptr<DecoderBuffer> decoder_buffer;
  if (!metadata.dropped_frame()) {
    decoder_buffer = DecoderBuffer::FromSharedMemoryRegion(
        it->second.Duplicate(), 0u /* offset */, metadata.payload_size_bytes);
    if (!decoder_buffer) {
      return nullptr;
    }
    decoder_buffer->set_timestamp(base::Microseconds(frame_index_));
  }

  auto source_timestamp_it = source_timestamps_.find(metadata.timestamp);
  LOG_ASSERT(source_timestamp_it != source_timestamps_.end());

  return BitstreamProcessor::BitstreamRef::Create(
      std::move(decoder_buffer), metadata, bitstream_buffer_id,
      source_timestamp_it->second,
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&VideoEncoderClient::BitstreamBufferProcessed,
                         weak_this_, bitstream_buffer_id)));
}

void VideoEncoderClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4) << "frame_index=" << frame_index_
            << ", encoded image size=" << metadata.payload_size_bytes
            << (metadata.dropped_frame() ? " (Drop Frame)" : "");
  {
    // |metadata.payload_size_bytes| can be zero here, but counts the dropped
    // frame to compute a bitrate from the network point of view.
    base::AutoLock auto_lock(stats_lock_);
    current_stats_.total_num_encoded_frames++;
    current_stats_.total_encoded_frames_size += metadata.payload_size_bytes;
    if (metadata.dropped_frame()) {
      current_stats_.num_dropped_frames++;
    } else {
      if (metadata.vp9.has_value()) {
        uint8_t temporal_id = metadata.vp9->temporal_idx;
        uint8_t spatial_id = metadata.vp9->spatial_idx;
        ASSERT_LT(spatial_id, current_stats_.num_spatial_layers);
        ASSERT_LT(temporal_id, current_stats_.num_temporal_layers);
        current_stats_.num_encoded_frames_per_layer[spatial_id][temporal_id]++;
        current_stats_.encoded_frames_size_per_layer[spatial_id][temporal_id] +=
            metadata.payload_size_bytes;
      } else if (metadata.h264.has_value()) {
        uint8_t temporal_id = metadata.h264->temporal_idx;
        ASSERT_EQ(current_stats_.num_spatial_layers, 1u);
        current_stats_.num_encoded_frames_per_layer[0][temporal_id]++;
        current_stats_.encoded_frames_size_per_layer[0][temporal_id] +=
            metadata.payload_size_bytes;
      } else if (metadata.vp8.has_value()) {
        uint8_t temporal_id = metadata.vp8->temporal_idx;
        ASSERT_EQ(current_stats_.num_spatial_layers, 1u);
        current_stats_.num_encoded_frames_per_layer[0][temporal_id]++;
        current_stats_.encoded_frames_size_per_layer[0][temporal_id] +=
            metadata.payload_size_bytes;
      }
    }
  }

  auto it = bitstream_buffers_.find(bitstream_buffer_id);
  ASSERT_NE(it, bitstream_buffers_.end());
  if (metadata.key_frame)
    FireEvent(VideoEncoder::EncoderEvent::kKeyFrame);

  // Notify the test an encoded bitstream buffer is ready. We should only do
  // this after scheduling the bitstream to be processed, so calling
  // WaitForBitstreamProcessors() after receiving this event will always
  // guarantee the bitstream to be processed.
  FireEvent(VideoEncoder::EncoderEvent::kBitstreamReady);

  if (bitstream_processors_.empty()) {
    BitstreamBufferProcessed(bitstream_buffer_id);
  } else {
    auto bitstream_ref = CreateBitstreamRef(bitstream_buffer_id, metadata);
    ASSERT_TRUE(bitstream_ref);
    for (auto& bitstream_processor_ : bitstream_processors_) {
      bitstream_processor_->ProcessBitstream(bitstream_ref, frame_index_);
    }
  }

  if (metadata.end_of_picture()) {
    frame_index_++;
    CHECK_EQ(source_timestamps_.erase(metadata.timestamp), 1u);
  }
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

void VideoEncoderClient::NotifyErrorStatus(const EncoderStatus& status) {
  ASSERT_FALSE(status.is_ok());
  LOG(ERROR) << "NotifyErrorStatus() is called, code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  FireEvent(VideoEncoder::EncoderEvent::kError);
}

void VideoEncoderClient::NotifyEncoderInfoChange(const VideoEncoderInfo& info) {
}

void VideoEncoderClient::CreateEncoderTask(const RawVideo* video,
                                           bool* success,
                                           base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_EQ(encoder_client_state_, VideoEncoderClientState::kUninitialized);
  ASSERT_TRUE(!encoder_) << "Can't create encoder: already created";
  ASSERT_TRUE(video);

  video_ = video;

  VideoEncodeAccelerator::Config config(
      video_->PixelFormat(), encoder_client_config_.output_resolution,
      encoder_client_config_.output_profile,
      encoder_client_config_.bitrate_allocation.GetSumBitrate(),
      encoder_client_config_.framerate,
      encoder_client_config_.input_storage_type,
      encoder_client_config_.content_type);

  config.drop_frame_thresh_percentage =
      encoder_client_config_.drop_frame_thresh;
  config.spatial_layers = encoder_client_config_.spatial_layers;
  config.inter_layer_pred = encoder_client_config_.inter_layer_pred_mode;

  encoder_ = GpuVideoEncodeAcceleratorFactory::CreateVEA(
      config, this, gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
      gpu::GPUInfo::GPUDevice());
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

  if (aligned_data_helper_->AtEndOfStream()) {
    // Flush immediately when we reached the end of the stream (either the real
    // end, or the artificial end when using num_encode_frames). This changes
    // the state to kFlushing so further encode tasks will be aborted.
    FlushTask();
    return;
  }

  scoped_refptr<VideoFrame> video_frame = aligned_data_helper_->GetNextFrame();
  ASSERT_TRUE(video_frame);
  video_frame->AddDestructionObserver(base::BindOnce(
      CallbackThunk<decltype(&VideoEncoderClient::EncodeDoneTask),
                    base::TimeDelta>,
      weak_this_, encoder_client_task_runner_,
      &VideoEncoderClient::EncodeDoneTask, video_frame->timestamp()));
  source_timestamps_[video_frame->timestamp()] = base::TimeTicks::Now();

  encoder_->Encode(video_frame, force_keyframe_);

  force_keyframe_ = false;
  num_encodes_requested_++;
  num_outstanding_encode_requests_++;
  if (encoder_client_config_.encode_interval) {
    // Schedules the next encode here if we're encoding at a fixed ratio.
    // Otherwise the next encode will be scheduled immediately when the previous
    // operation is done in EncodeDoneTask().
    encoder_client_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&VideoEncoderClient::EncodeNextFrameTask, weak_this_),
        *encoder_client_config_.encode_interval);
  }
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
  aligned_data_helper_->UpdateFrameRate(framerate);
  encoder_->RequestEncodingParametersChange(bitrate, framerate, std::nullopt);
  base::AutoLock auto_lcok(stats_lock_);
  current_stats_.framerate = framerate;
}

void VideoEncoderClient::ForceKeyFrameTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4);

  force_keyframe_ = true;
}

void VideoEncoderClient::EncodeDoneTask(base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4);

  FireEvent(VideoEncoder::EncoderEvent::kFrameReleased);

  num_outstanding_encode_requests_--;
  FlushDoneTaskIfNeeded();

  if (!encoder_client_config_.encode_interval) {
    // Queue the next frame to be encoded.
    encoder_client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoEncoderClient::EncodeNextFrameTask, weak_this_));
  }
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
