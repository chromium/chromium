// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/bitstream_validator.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
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
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors) {
  std::unique_ptr<VideoDecoder> decoder;
  decoder = CreateDecoder(decoder_config.codec());
  if (!decoder)
    return nullptr;

  auto validator = base::WrapUnique(new BitstreamValidator(
      std::move(decoder), last_frame_index, std::move(video_frame_processors)));
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
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors)
    : decoder_(std::move(decoder)),
      last_frame_index_(last_frame_index),
      video_frame_processors_(std::move(video_frame_processors)),
      validator_thread_("BitstreamValidatorThread"),
      validator_cv_(&validator_lock_),
      num_buffers_validating_(0),
      decode_error_(false) {
  DETACH_FROM_SEQUENCE(validator_sequence_checker_);
  DETACH_FROM_SEQUENCE(validator_thread_sequence_checker_);
}

BitstreamValidator::~BitstreamValidator() = default;

void BitstreamValidator::ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                                          size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_sequence_checker_);
  base::AutoLock lock(validator_lock_);
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
  scoped_refptr<DecoderBuffer> buffer = bitstream->buffer;
  int64_t timestamp = buffer->timestamp().InMicroseconds();
  decoding_buffers_.Put(timestamp,
                        std::make_pair(frame_index, std::move(bitstream)));
  // Validate the encoded bitstream buffer by decoding its contents using a
  // software decoder.
  decoder_->Decode(std::move(buffer),
                   base::BindOnce(&BitstreamValidator::DecodeDone,
                                  base::Unretained(this), timestamp));

  if (frame_index == last_frame_index_) {
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

  // Send the decoded frame to the configured video frame processors to perform
  // additional verification.
  for (const auto& processor : video_frame_processors_)
    processor->ProcessVideoFrame(frame, frame_index);

  base::AutoLock lock(validator_lock_);
  num_buffers_validating_--;
  validator_cv_.Signal();
}

bool BitstreamValidator::WaitUntilDone() {
  base::AutoLock auto_lock(validator_lock_);
  while (num_buffers_validating_ > 0)
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
