// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/bitstream_validator.h"

#include <numeric>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/filters/dav1d_video_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"
#include "media/gpu/test/video_frame_helpers.h"

namespace media {
namespace test {
namespace {

constexpr int64_t kEOSTimeStamp = -1;

std::unique_ptr<VideoDecoder> CreateDecoder(
    VideoCodec codec,
    std::unique_ptr<MediaLog>* media_log) {
  std::unique_ptr<VideoDecoder> decoder;

  if (codec == VideoCodec::kAV1) {
#if BUILDFLAG(ENABLE_DAV1D_DECODER)
    *media_log = std::make_unique<NullMediaLog>();
    decoder = std::make_unique<Dav1dVideoDecoder>((*media_log)->Clone());
#endif
  }

  if (codec == VideoCodec::kVP8 || codec == VideoCodec::kVP9) {
#if BUILDFLAG(ENABLE_LIBVPX)
    decoder = std::make_unique<VpxVideoDecoder>();
#endif
  }

  if (!decoder) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    *media_log = std::make_unique<NullMediaLog>();
    decoder = std::make_unique<FFmpegVideoDecoder>(media_log->get());
#endif
  }

  return decoder;
}
}  // namespace

// static
std::unique_ptr<BitstreamValidator> BitstreamValidator::Create(
    const VideoDecoderConfig& decoder_config,
    size_t last_frame_index,
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors,
    std::optional<size_t> spatial_layer_index_to_decode,
    std::optional<size_t> temporal_layer_index_to_decode,
    const std::vector<gfx::Size>& spatial_layer_resolutions) {
  std::unique_ptr<MediaLog> media_log;
  auto decoder = CreateDecoder(decoder_config.codec(), &media_log);
  if (!decoder)
    return nullptr;

  auto validator = base::WrapUnique(new BitstreamValidator(
      std::move(decoder), std::move(media_log), last_frame_index,
      decoder_config.visible_rect(), spatial_layer_index_to_decode,
      temporal_layer_index_to_decode, spatial_layer_resolutions,
      std::move(video_frame_processors)));
  if (!validator->Initialize(decoder_config))
    return nullptr;

  return validator;
}

bool BitstreamValidator::Initialize(const VideoDecoderConfig& decoder_config) {
  DCHECK(decoder_);
  if (!validator_thread_.Start()) {
    LOG(ERROR) << "Failed to start frame validator thread";
    return false;
  }

  bool success = false;
  base::WaitableEvent initialized;
  VideoDecoder::InitCB init_done = base::BindOnce(
      [](bool* result, base::WaitableEvent* initialized, DecoderStatus status) {
        *result = true;
        if (!status.is_ok()) {
          LOG(ERROR) << "Failed decoder initialization ("
                     << static_cast<int32_t>(status.code())
                     << "): " << status.message();
        }
        initialized->Signal();
      },
      &success, &initialized);
  validator_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BitstreamValidator::InitializeVideoDecoder,
                                base::Unretained(this), decoder_config,
                                std::move(init_done)));
  initialized.Wait();
  return success;
}

void BitstreamValidator::InitializeVideoDecoder(
    const VideoDecoderConfig& decoder_config,
    VideoDecoder::InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  decoder_->Initialize(
      decoder_config, false, nullptr, std::move(init_cb),
      base::BindRepeating(&BitstreamValidator::VerifyOutputFrame,
                          base::Unretained(this)),
      base::NullCallback());
}

BitstreamValidator::BitstreamValidator(
    std::unique_ptr<VideoDecoder> decoder,
    std::unique_ptr<MediaLog> media_log,
    size_t last_frame_index,
    const gfx::Rect& decoding_rect,
    std::optional<size_t> spatial_layer_index_to_decode,
    std::optional<size_t> temporal_layer_index_to_decode,
    const std::vector<gfx::Size>& spatial_layer_resolutions,
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors)
    : decoder_(std::move(decoder)),
      media_log_(std::move(media_log)),
      last_frame_index_(last_frame_index),
      desired_decoding_rect_(decoding_rect),
      spatial_layer_index_to_decode_(spatial_layer_index_to_decode),
      temporal_layer_index_to_decode_(temporal_layer_index_to_decode),
      spatial_layer_resolutions_(spatial_layer_resolutions),
      video_frame_processors_(std::move(video_frame_processors)),
      validator_thread_("BitstreamValidatorThread"),
      validator_cv_(&validator_lock_),
      num_buffers_validating_(0),
      decode_error_(false),
      waiting_flush_done_(false) {
  DETACH_FROM_SEQUENCE(validator_sequence_checker_);
  DETACH_FROM_SEQUENCE(validator_thread_sequence_checker_);
}

BitstreamValidator::~BitstreamValidator() {
  // Make sure no buffer is being validated and processed.
  WaitUntilDone();

  // Since |decoder_| has to be destroyed on the sequence that executes
  // Initialize(). Destroys it on the validator thread task runner.
  if (validator_thread_.IsRunning())
    validator_thread_.task_runner()->DeleteSoon(FROM_HERE, std::move(decoder_));
}

void BitstreamValidator::ConstructSpatialIndices(
    const std::vector<gfx::Size>& spatial_layer_resolutions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  CHECK(!spatial_layer_resolutions.empty());
  CHECK_LE(spatial_layer_resolutions.size(), spatial_layer_resolutions_.size());

  original_spatial_indices_.resize(spatial_layer_resolutions.size());
  auto begin = base::ranges::find(spatial_layer_resolutions_,
                                  spatial_layer_resolutions.front());
  CHECK(begin != spatial_layer_resolutions_.end());
  uint8_t sid_offset = begin - spatial_layer_resolutions_.begin();
  for (size_t i = 0; i < spatial_layer_resolutions.size(); ++i) {
    CHECK_LT(sid_offset + i, spatial_layer_resolutions_.size());
    CHECK_EQ(spatial_layer_resolutions[i],
             spatial_layer_resolutions_[sid_offset + i]);
    original_spatial_indices_[i] = sid_offset + i;
  }
}

void BitstreamValidator::ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                                          size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_sequence_checker_);
  LOG_ASSERT(frame_index <= last_frame_index_)
      << "frame_index is larger than last frame index, frame_index="
      << frame_index << ", last_frame_index_=" << last_frame_index_;
  if (bitstream->metadata.dropped_frame()) {
    // Drop frame. Do nothing.
    return;
  }
  base::AutoLock lock(validator_lock_);
  // If many pending buffers are accumulated in this validator class and the
  // allocated memory size becomes large, the test process is killed by the
  // system due to out of memory.
  // To avoid the issue, wait until the number of buffers being validated is
  // less than or equal to 16. The number is arbitrary chosen.
  if (frame_index != last_frame_index_) {
    constexpr size_t kMaxNumPendingBuffers = 16;
    while (num_buffers_validating_ > kMaxNumPendingBuffers)
      validator_cv_.Wait();
  }

  num_buffers_validating_++;
  validator_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BitstreamValidator::ProcessBitstreamTask,
                                base::Unretained(this), std::move(bitstream),
                                frame_index));
}

void BitstreamValidator::ProcessBitstreamTask(
    scoped_refptr<BitstreamRef> bitstream,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  bool should_decode = false;
  bool should_flush = false;
  if (!spatial_layer_index_to_decode_ && !temporal_layer_index_to_decode_) {
    should_decode = true;
    should_flush = frame_index == last_frame_index_;
  } else if (bitstream->metadata.vp9) {
    const Vp9Metadata& metadata = *bitstream->metadata.vp9;
    if (bitstream->metadata.key_frame)
      ConstructSpatialIndices(metadata.spatial_layer_resolutions);

    const uint8_t spatial_idx = original_spatial_indices_[metadata.spatial_idx];
    // |should_decode| equals true if SVC encoding mode with corresponding
    // spatial/temporal decode chain.
    // Check the spatial layer index.
    should_decode = (spatial_idx == *spatial_layer_index_to_decode_) ||
                    (spatial_idx < *spatial_layer_index_to_decode_ &&
                     metadata.referenced_by_upper_spatial_layers);
    // Check the temporal layer index.
    should_decode &= metadata.temporal_idx <= *temporal_layer_index_to_decode_;
    // |should_flush| is true if the last frame is received regardless whether
    // the frame is decoded.
    should_flush = frame_index == last_frame_index_ &&
                   spatial_idx == original_spatial_indices_.size() - 1;
  } else if (bitstream->metadata.h264) {
    const H264Metadata& metadata = *bitstream->metadata.h264;
    should_decode = metadata.temporal_idx <= *temporal_layer_index_to_decode_;
    should_flush = frame_index == last_frame_index_;
  } else if (bitstream->metadata.vp8) {
    const Vp8Metadata& metadata = *bitstream->metadata.vp8;
    should_decode = metadata.temporal_idx <= *temporal_layer_index_to_decode_;
    should_flush = frame_index == last_frame_index_;
  }

  if (should_flush) {
    // |waiting_flush_done_| should be set before calling Decode() as
    // VideoDecoder::OutputCB (here, VerifyOutputFrame) might be called
    // synchronously.
    base::AutoLock lock(validator_lock_);
    waiting_flush_done_ = true;
  }

  if (should_decode) {
    scoped_refptr<DecoderBuffer> buffer = bitstream->buffer;
    int64_t timestamp = buffer->timestamp().InMicroseconds();
    decoding_buffers_.Put(timestamp,
                          std::make_pair(frame_index, std::move(bitstream)));
    // Validate the encoded bitstream buffer by decoding its contents using a
    // software decoder.
    decoder_->Decode(std::move(buffer),
                     base::BindOnce(&BitstreamValidator::DecodeDone,
                                    base::Unretained(this), timestamp));
  } else {
    // Skip |bitstream| because it contains a frame in upper layers than layers
    // to be validated.
    base::AutoLock lock(validator_lock_);
    num_buffers_validating_--;
    validator_cv_.Signal();
  }

  if (should_flush) {
    // Flush pending buffers.
    decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                     base::BindOnce(&BitstreamValidator::DecodeDone,
                                    base::Unretained(this), kEOSTimeStamp));
  }
}

void BitstreamValidator::DecodeDone(int64_t timestamp, DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  if (!status.is_ok()) {
    base::AutoLock lock(validator_lock_);
    if (!decode_error_) {
      decode_error_ = true;
      LOG(ERROR) << "DecodeStatus is not OK, status="
                 << static_cast<int>(status.code());
    }
  }
  if (timestamp == kEOSTimeStamp) {
    base::AutoLock lock(validator_lock_);
    waiting_flush_done_ = false;
    validator_cv_.Signal();
    return;
  }
}

void BitstreamValidator::OutputFrameProcessed() {
  // This function can be called on any sequence because the written variables
  // are guarded by a lock.
  base::AutoLock lock(validator_lock_);
  num_buffers_validating_--;
  validator_cv_.Signal();
}

void BitstreamValidator::VerifyOutputFrame(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  auto it = decoding_buffers_.Peek(frame->timestamp().InMicroseconds());
  if (it == decoding_buffers_.end()) {
    LOG(WARNING) << "Unexpected timestamp: "
                 << frame->timestamp().InMicroseconds();
    return;
  }
  size_t frame_index = it->second.first;
  decoding_buffers_.Erase(it);

  // For k-SVC stream, we need to decode the spatial layer frames up to the
  // validated spatial layer in key picture. We don't validate the lower spatial
  // layer frames as they are not shown frames. Skip them.
  if (frame->visible_rect() != desired_decoding_rect_) {
    if (!spatial_layer_index_to_decode_ ||
        *spatial_layer_index_to_decode_ == 0) {
      LOG(ERROR) << __func__ << " Unexpected frame skip";
    }
    DVLOGF(3) << "Skip a frame to be not shown. visible_rect="
              << frame->visible_rect().ToString()
              << ", shown visible_rect=" << desired_decoding_rect_.ToString();

    OutputFrameProcessed();
    return;
  }

  // Wraps VideoFrame because the reference of |frame| might be kept in
  // VideoDecoder and thus |frame| is not released unless |decoder_| is
  // destructed.
  auto wrapped_video_frame =
      VideoFrame::WrapVideoFrame(frame, frame->format(), frame->visible_rect(),
                                 frame->visible_rect().size());
  LOG_ASSERT(wrapped_video_frame) << "Failed creating a wrapped VideoFrame";
  wrapped_video_frame->AddDestructionObserver(base::BindOnce(
      &BitstreamValidator::OutputFrameProcessed, base::Unretained(this)));
  // Send the decoded frame to the configured video frame processors to perform
  // additional verification.
  for (const auto& processor : video_frame_processors_)
    processor->ProcessVideoFrame(wrapped_video_frame, frame_index);
}

bool BitstreamValidator::WaitUntilDone() {
  base::AutoLock auto_lock(validator_lock_);
  while (num_buffers_validating_ > 0 || waiting_flush_done_)
    validator_cv_.Wait();

  bool success = true;
  for (const auto& processor : video_frame_processors_) {
    if (!processor->WaitUntilDone()) {
      LOG(ERROR) << "VideoFrameProcessor error";
      success = false;
    }
  }

  if (decode_error_) {
    LOG(ERROR) << "VideoDecoder error";
    success = false;
  }
  return success;
}
}  // namespace test
}  // namespace media
