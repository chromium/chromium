// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/bitstream_validator.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"
#include "media/gpu/test/video_frame_helpers.h"

namespace media {
namespace test {
namespace {

constexpr int64_t kEOSTimeStamp = -1;

std::unique_ptr<VideoDecoder> CreateDecoder(VideoCodec codec) {
  std::unique_ptr<VideoDecoder> decoder;

  if (codec == kCodecVP8 || codec == kCodecVP9) {
#if BUILDFLAG(ENABLE_LIBVPX)
    LOG_ASSERT(!base::FeatureList::IsEnabled(kFFmpegDecodeOpaqueVP8));
    decoder = std::make_unique<VpxVideoDecoder>();
#endif
  }

  if (!decoder) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    decoder = std::make_unique<FFmpegVideoDecoder>(nullptr);
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
    base::Optional<size_t> num_vp9_temporal_layers_to_decode) {
  std::unique_ptr<VideoDecoder> decoder;
  decoder = CreateDecoder(decoder_config.codec());
  if (!decoder)
    return nullptr;

  auto validator = base::WrapUnique(new BitstreamValidator(
      std::move(decoder), last_frame_index, num_vp9_temporal_layers_to_decode,
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
      [](bool* result, base::WaitableEvent* initialized, Status status) {
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
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
  decoder_->Initialize(
      decoder_config, false, nullptr, std::move(init_cb),
      base::BindRepeating(&BitstreamValidator::VerifyOutputFrame,
                          base::Unretained(this)),
      base::NullCallback());
}

BitstreamValidator::BitstreamValidator(
    std::unique_ptr<VideoDecoder> decoder,
    size_t last_frame_index,
    base::Optional<size_t> num_vp9_temporal_layers_to_decode,
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors)
    : decoder_(std::move(decoder)),
      last_frame_index_(last_frame_index),
      num_vp9_temporal_layers_to_decode_(num_vp9_temporal_layers_to_decode),
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

void BitstreamValidator::ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                                          size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_sequence_checker_);
  LOG_ASSERT(frame_index <= last_frame_index_)
      << "frame_index is larger than last frame index, frame_index="
      << frame_index << ", last_frame_index_=" << last_frame_index_;
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
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
  const bool should_decode = !num_vp9_temporal_layers_to_decode_ ||
                             (bitstream->metadata.vp9->temporal_idx <
                              *num_vp9_temporal_layers_to_decode_);
  const bool should_flush = frame_index == last_frame_index_;

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

void BitstreamValidator::DecodeDone(int64_t timestamp, Status status) {
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
  if (!status.is_ok()) {
    base::AutoLock lock(validator_lock_);
    if (!decode_error_) {
      decode_error_ = true;
      LOG(ERROR) << "DecodeStatus is not OK, status="
                 << GetDecodeStatusString(status.code());
    }
  }
  if (timestamp == kEOSTimeStamp) {
    base::AutoLock lock(validator_lock_);
    waiting_flush_done_ = false;
    validator_cv_.Signal();
    return;
  }

  // This validator and |decoder_| don't use bitstream any more. Release here,
  // so that a caller can use the bitstream buffer and proceed.
  auto it = decoding_buffers_.Peek(timestamp);
  if (it == decoding_buffers_.end()) {
    // This occurs when VerifyfOutputFrame() is called before DecodeDone() and
    // the entry has been deleted.
    return;
  }
  it->second.second.reset();
}

void BitstreamValidator::OutputFrameProcessed() {
  // This function can be called on any sequence because the written variables
  // are guarded by a lock.
  base::AutoLock lock(validator_lock_);
  num_buffers_validating_--;
  validator_cv_.Signal();
}

void BitstreamValidator::VerifyOutputFrame(scoped_refptr<VideoFrame> frame) {
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
  auto it = decoding_buffers_.Peek(frame->timestamp().InMicroseconds());
  if (it == decoding_buffers_.end()) {
    LOG(WARNING) << "Unexpected timestamp: "
                 << frame->timestamp().InMicroseconds();
    return;
  }
  size_t frame_index = it->second.first;
  decoding_buffers_.Erase(it);

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
