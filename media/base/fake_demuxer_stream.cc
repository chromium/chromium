// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/fake_demuxer_stream.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

const int kStartTimestampMs = 0;
const int kDurationMs = 30;
const int kDefaultStartWidth = 320;
const int kDefaultStartHeight = 240;
const int kDefaultWidthDelta = 4;
const int kDefaultHeightDelta = 3;
const uint8_t kKeyId[] = {0x00, 0x01, 0x02, 0x03};
const uint8_t kIv[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

FakeDemuxerStream::FakeDemuxerStream(int num_configs,
                                     int num_buffers_in_one_config,
                                     bool is_encrypted)
    : FakeDemuxerStream(
          num_configs,
          num_buffers_in_one_config,
          is_encrypted,
          gfx::Size(kDefaultStartWidth, kDefaultStartHeight),
          gfx::Vector2dF(kDefaultWidthDelta, kDefaultHeightDelta)) {}

FakeDemuxerStream::FakeDemuxerStream(int num_configs,
                                     int num_buffers_in_one_config,
                                     bool is_encrypted,
                                     gfx::Size start_coded_size,
                                     gfx::Vector2dF coded_size_delta)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      num_configs_(num_configs),
      num_buffers_in_one_config_(num_buffers_in_one_config),
      config_changes_(num_configs > 1),
      is_encrypted_(is_encrypted),
      start_coded_size_(start_coded_size),
      coded_size_delta_(coded_size_delta),
      read_to_hold_(-1) {
  DCHECK_GT(num_configs, 0);
  DCHECK_GT(num_buffers_in_one_config, 0);
  Initialize();
  UpdateVideoDecoderConfig();
}

FakeDemuxerStream::~FakeDemuxerStream() = default;

void FakeDemuxerStream::Initialize() {
  DCHECK_EQ(-1, read_to_hold_);
  num_configs_left_ = num_configs_;
  num_buffers_left_in_current_config_ = num_buffers_in_one_config_;
  num_buffers_returned_ = 0;
  current_timestamp_ = base::Milliseconds(kStartTimestampMs);
  duration_ = base::Milliseconds(kDurationMs);
  next_size_ = start_coded_size_;
  next_read_num_ = 0;
}

void FakeDemuxerStream::Read(uint32_t count, ReadCB read_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!read_cb_);

  read_cb_ = base::BindPostTaskToCurrentDefault(std::move(read_cb));

  if (read_to_hold_ == next_read_num_)
    return;

  DCHECK(read_to_hold_ == -1 || read_to_hold_ > next_read_num_);
  DoRead(count);
}

AudioDecoderConfig FakeDemuxerStream::audio_decoder_config() {
  NOTREACHED();
}

VideoDecoderConfig FakeDemuxerStream::video_decoder_config() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return video_decoder_config_;
}

// TODO(xhwang): Support audio if needed.
DemuxerStream::Type FakeDemuxerStream::type() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return VIDEO;
}

bool FakeDemuxerStream::SupportsConfigChanges() {
  return config_changes_;
}

void FakeDemuxerStream::HoldNextRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  read_to_hold_ = next_read_num_;
}

void FakeDemuxerStream::HoldNextConfigChangeRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Set |read_to_hold_| to be the next config change read.
  read_to_hold_ = next_read_num_ + num_buffers_in_one_config_ -
                  next_read_num_ % (num_buffers_in_one_config_ + 1);
}

void FakeDemuxerStream::SatisfyRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(read_to_hold_, next_read_num_);
  DCHECK(read_cb_);

  read_to_hold_ = -1;
  DoRead();
}

void FakeDemuxerStream::SatisfyReadAndHoldNext() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(read_to_hold_, next_read_num_);
  DCHECK(read_cb_);

  ++read_to_hold_;
  DoRead();
}

void FakeDemuxerStream::Reset() {
  read_to_hold_ = -1;

  if (read_cb_)
    std::move(read_cb_).Run(kAborted, {});
}

void FakeDemuxerStream::Error() {
  read_to_hold_ = -1;

  if (read_cb_)
    std::move(read_cb_).Run(kError, {});
}

void FakeDemuxerStream::SeekToStart() {
  Reset();
  Initialize();
}

void FakeDemuxerStream::SeekToEndOfStream() {
  num_configs_left_ = 0;
  num_buffers_left_in_current_config_ = 0;
}

void FakeDemuxerStream::UpdateVideoDecoderConfig() {
  const gfx::Rect kVisibleRect(next_size_.width(), next_size_.height());
  video_decoder_config_.Initialize(
      VideoCodec::kVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, next_size_, kVisibleRect, next_size_, EmptyExtraData(),
      is_encrypted_ ? EncryptionScheme::kCenc : EncryptionScheme::kUnencrypted);
  next_size_.set_width(next_size_.width() + coded_size_delta_.x());
  next_size_.set_height(next_size_.height() + coded_size_delta_.y());
}

void FakeDemuxerStream::DoRead(int read_count) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(read_cb_);
  DCHECK_GE(read_count, 1);

  next_read_num_++;

  if (num_buffers_left_in_current_config_ == 0) {
    // End of stream.
    if (num_configs_left_ == 0) {
      std::move(read_cb_).Run(kOk, {DecoderBuffer::CreateEOSBuffer()});
      return;
    }

    // Config change.
    num_buffers_left_in_current_config_ = num_buffers_in_one_config_;
    UpdateVideoDecoderConfig();
    std::move(read_cb_).Run(kConfigChanged, {});
    return;
  }

  DemuxerStream::DecoderBufferVector output_buffers;
  for (int i = 0; i < read_count; ++i) {
    scoped_refptr<DecoderBuffer> buffer = CreateFakeVideoBufferForTest(
        video_decoder_config_, current_timestamp_, duration_);

    // TODO(xhwang): Output out-of-order buffers if needed.
    if (is_encrypted_) {
      buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
          std::string(kKeyId, kKeyId + std::size(kKeyId)),
          std::string(kIv, kIv + std::size(kIv)),
          std::vector<SubsampleEntry>()));
    }
    buffer->set_timestamp(current_timestamp_);
    buffer->set_duration(duration_);
    current_timestamp_ += duration_;

    num_buffers_left_in_current_config_--;
    if (num_buffers_left_in_current_config_ == 0) {
      num_configs_left_--;
    }

    num_buffers_returned_++;
    output_buffers.emplace_back(std::move(buffer));
  }
  std::move(read_cb_).Run(kOk, std::move(output_buffers));
}

FakeMediaResource::FakeMediaResource(int num_video_configs,
                                     int num_video_buffers_in_one_config,
                                     bool is_video_encrypted)
    : fake_video_stream_(num_video_configs,
                         num_video_buffers_in_one_config,
                         is_video_encrypted) {}

FakeMediaResource::~FakeMediaResource() = default;

std::vector<DemuxerStream*> FakeMediaResource::GetAllStreams() {
  std::vector<DemuxerStream*> result;
  result.push_back(&fake_video_stream_);
  return result;
}

}  // namespace media
