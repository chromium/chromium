// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fake_video_decoder.h"

#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "media/base/test_helpers.h"

namespace media {

FakeVideoDecoder::FakeVideoDecoder(int decoder_id,
                                   int decoding_delay,
                                   int max_parallel_decoding_requests,
                                   const BytesDecodedCB& bytes_decoded_cb)
    : decoder_id_(decoder_id),
      decoding_delay_(decoding_delay),
      max_parallel_decoding_requests_(max_parallel_decoding_requests),
      bytes_decoded_cb_(bytes_decoded_cb),
      state_(STATE_UNINITIALIZED),
      hold_decode_(false),
      total_bytes_decoded_(0),
      fail_to_initialize_(false) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DVLOG(1) << decoder_id_ << ": " << __func__;
  DCHECK_GE(decoding_delay, 0);
}

FakeVideoDecoder::~FakeVideoDecoder() {
  DVLOG(1) << decoder_id_ << ": " << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == STATE_UNINITIALIZED)
    return;

  if (!init_cb_.IsNull())
    SatisfyInit();
  if (!held_decode_callbacks_.empty())
    SatisfyDecode();
  if (!reset_cb_.IsNull())
    SatisfyReset();

  decoded_frames_.clear();
  total_decoded_frames_ = 0;
}

void FakeVideoDecoder::EnableEncryptedConfigSupport() {
  supports_encrypted_config_ = true;
}

void FakeVideoDecoder::SetIsPlatformDecoder(bool value) {
  is_platform_decoder_ = value;
}

base::WeakPtr<FakeVideoDecoder> FakeVideoDecoder::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeVideoDecoder::SupportsDecryption() const {
  return supports_encrypted_config_;
}

bool FakeVideoDecoder::IsPlatformDecoder() const {
  return is_platform_decoder_;
}

VideoDecoderType FakeVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kTesting;
}

void FakeVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  CdmContext* cdm_context,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& waiting_cb) {
  DVLOG(1) << decoder_id_ << ": " << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());
  DCHECK(held_decode_callbacks_.empty())
      << "No reinitialization during pending decode.";
  DCHECK(reset_cb_.IsNull()) << "No reinitialization during pending reset.";

  current_config_ = config;
  init_cb_.SetCallback(base::BindPostTaskToCurrentDefault(std::move(init_cb)));

  // Don't need base::BindPostTaskToCurrentDefault() because |output_cb_| is
  // only called from RunDecodeCallback() which is posted from Decode().
  output_cb_ = output_cb;

  if (!decoded_frames_.empty()) {
    DVLOG(1) << "Decoded frames dropped during reinitialization.";
    decoded_frames_.clear();
  }

  if (config.is_encrypted() && (!supports_encrypted_config_ || !cdm_context)) {
    DVLOG(1) << "Encrypted config not supported.";
    state_ = STATE_NORMAL;
    init_cb_.RunOrHold(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (fail_to_initialize_) {
    DVLOG(1) << decoder_id_ << ": Initialization failed.";
    state_ = STATE_ERROR;
    init_cb_.RunOrHold(DecoderStatus::Codes::kFailed);
  } else {
    DVLOG(1) << decoder_id_ << ": Initialization succeeded.";
    state_ = STATE_NORMAL;
    init_cb_.RunOrHold(DecoderStatus::Codes::kOk);
  }
}

void FakeVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(reset_cb_.IsNull());
  DCHECK_LE(decoded_frames_.size(),
            decoding_delay_ + held_decode_callbacks_.size());
  DCHECK_LT(static_cast<int>(held_decode_callbacks_.size()),
            max_parallel_decoding_requests_);
  DCHECK_NE(state_, STATE_END_OF_STREAM);

  int buffer_size = buffer->end_of_stream() ? 0 : buffer->size();
  DecodeCB wrapped_decode_cb = base::BindOnce(
      &FakeVideoDecoder::OnFrameDecoded, weak_factory_.GetWeakPtr(),
      buffer_size, base::BindPostTaskToCurrentDefault(std::move(decode_cb)));

  if (state_ == STATE_ERROR) {
    std::move(wrapped_decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (buffer->end_of_stream()) {
    state_ = STATE_END_OF_STREAM;
    if (buffer->next_config()) {
      eos_next_configs_.emplace_back(
          absl::get<VideoDecoderConfig>(*buffer->next_config()));
    }
  } else {
    DCHECK(VerifyFakeVideoBufferForTest(*buffer, current_config_));
    decoded_frames_.push_back(MakeVideoFrame(*buffer));
  }

  RunOrHoldDecode(std::move(wrapped_decode_cb));
}

scoped_refptr<VideoFrame> FakeVideoDecoder::MakeVideoFrame(
    const DecoderBuffer& buffer) {
  auto frame = VideoFrame::CreateVideoHoleFrame(base::UnguessableToken(),
                                                current_config_.coded_size(),
                                                buffer.timestamp());
  DCHECK(frame);
  return frame;
}

void FakeVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(reset_cb_.IsNull());

  reset_cb_.SetCallback(base::BindPostTaskToCurrentDefault(std::move(closure)));
  decoded_frames_.clear();

  // Defer the reset if a decode is pending.
  if (!held_decode_callbacks_.empty())
    return;

  DoReset();
}

void FakeVideoDecoder::HoldNextInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_cb_.HoldCallback();
}

void FakeVideoDecoder::HoldDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hold_decode_ = true;
}

void FakeVideoDecoder::HoldNextReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reset_cb_.HoldCallback();
}

void FakeVideoDecoder::SatisfyInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(held_decode_callbacks_.empty());
  DCHECK(reset_cb_.IsNull());

  init_cb_.RunHeldCallback();
}

void FakeVideoDecoder::SatisfyDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(hold_decode_);

  hold_decode_ = false;

  while (!held_decode_callbacks_.empty()) {
    SatisfySingleDecode();
  }
}

void FakeVideoDecoder::SatisfySingleDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!held_decode_callbacks_.empty());

  DecodeCB decode_cb = std::move(held_decode_callbacks_.front());
  held_decode_callbacks_.pop_front();
  total_decoded_frames_++;
  RunDecodeCallback(std::move(decode_cb));

  if (!reset_cb_.IsNull() && held_decode_callbacks_.empty())
    DoReset();
}

void FakeVideoDecoder::SatisfyReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(held_decode_callbacks_.empty());
  reset_cb_.RunHeldCallback();
}

void FakeVideoDecoder::SimulateError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_ = STATE_ERROR;
  while (!held_decode_callbacks_.empty()) {
    std::move(held_decode_callbacks_.front())
        .Run(DecoderStatus::Codes::kFailed);
    held_decode_callbacks_.pop_front();
  }
  decoded_frames_.clear();
}

void FakeVideoDecoder::SimulateFailureToInit() {
  fail_to_initialize_ = true;
}

int FakeVideoDecoder::GetMaxDecodeRequests() const {
  return max_parallel_decoding_requests_;
}

void FakeVideoDecoder::OnFrameDecoded(int buffer_size,
                                      DecodeCB decode_cb,
                                      DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.is_ok()) {
    total_bytes_decoded_ += buffer_size;
    if (bytes_decoded_cb_)
      bytes_decoded_cb_.Run(buffer_size);
  }
  std::move(decode_cb).Run(std::move(status));
}

void FakeVideoDecoder::RunOrHoldDecode(DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hold_decode_) {
    held_decode_callbacks_.push_back(std::move(decode_cb));
  } else {
    DCHECK(held_decode_callbacks_.empty());
    RunDecodeCallback(std::move(decode_cb));
  }
}

void FakeVideoDecoder::RunDecodeCallback(DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!reset_cb_.IsNull()) {
    DCHECK(decoded_frames_.empty());
    std::move(decode_cb).Run(DecoderStatus::Codes::kAborted);
    return;
  }

  // Make sure we leave decoding_delay_ frames in the queue and also frames for
  // all pending decode callbacks, except the current one.
  if (decoded_frames_.size() >
      decoding_delay_ + held_decode_callbacks_.size()) {
    output_cb_.Run(decoded_frames_.front());
    decoded_frames_.pop_front();
  } else if (state_ == STATE_END_OF_STREAM) {
    // Drain the queue if this was the last request in the stream, otherwise
    // just pop the last frame from the queue.
    if (held_decode_callbacks_.empty()) {
      while (!decoded_frames_.empty()) {
        output_cb_.Run(decoded_frames_.front());
        decoded_frames_.pop_front();
      }
      state_ = STATE_NORMAL;
    } else if (!decoded_frames_.empty()) {
      output_cb_.Run(decoded_frames_.front());
      decoded_frames_.pop_front();
    }
  }

  std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
}

void FakeVideoDecoder::DoReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(held_decode_callbacks_.empty());
  DCHECK(!reset_cb_.IsNull());

  reset_cb_.RunOrHold();
}

}  // namespace media
