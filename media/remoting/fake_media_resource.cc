// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/fake_media_resource.h"

#include <vector>

#include "base/callback_helpers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media {
namespace remoting {

FakeDemuxerStream::FakeDemuxerStream(bool is_audio) {
  type_ = is_audio ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
  if (is_audio) {
    audio_config_.Initialize(kCodecAAC, kSampleFormatS16, CHANNEL_LAYOUT_STEREO,
                             38400, std::vector<uint8_t>(),
                             EncryptionScheme::kUnencrypted, base::TimeDelta(),
                             0);
  } else {
    gfx::Size size(640, 480);
    gfx::Rect rect(0, 0, 640, 480);
    video_config_.Initialize(kCodecH264, H264PROFILE_BASELINE,
                             VideoDecoderConfig::AlphaMode::kIsOpaque,
                             VideoColorSpace::REC601(), kNoTransformation, size,
                             rect, size, std::vector<uint8_t>(),
                             EncryptionScheme::kUnencrypted);
  }
  ON_CALL(*this, Read(_))
      .WillByDefault(Invoke(this, &FakeDemuxerStream::FakeRead));
}

FakeDemuxerStream::~FakeDemuxerStream() = default;

void FakeDemuxerStream::FakeRead(ReadCB read_cb) {
  if (buffer_queue_.empty()) {
    // Silent return to simulate waiting for buffer available.
    pending_read_cb_ = std::move(read_cb);
    return;
  }
  scoped_refptr<DecoderBuffer> buffer = buffer_queue_.front();
  buffer_queue_.pop_front();
  std::move(read_cb).Run(kOk, buffer);
}

bool FakeDemuxerStream::IsReadPending() const {
  return !pending_read_cb_.is_null();
}

AudioDecoderConfig FakeDemuxerStream::audio_decoder_config() {
  return audio_config_;
}

VideoDecoderConfig FakeDemuxerStream::video_decoder_config() {
  return video_config_;
}

DemuxerStream::Type FakeDemuxerStream::type() const {
  return type_;
}

DemuxerStream::Liveness FakeDemuxerStream::liveness() const {
  return LIVENESS_UNKNOWN;
}

bool FakeDemuxerStream::SupportsConfigChanges() {
  return false;
}

void FakeDemuxerStream::CreateFakeFrame(size_t size,
                                        bool key_frame,
                                        int pts_ms) {
  std::vector<uint8_t> buffer(size);
  // Assign each byte in the buffer its index mod 256.
  for (size_t i = 0; i < size; ++i) {
    buffer[i] = static_cast<uint8_t>(i & 0xFF);
  }
  base::TimeDelta pts = base::TimeDelta::FromMilliseconds(pts_ms);

  // To DecoderBuffer
  scoped_refptr<DecoderBuffer> input_buffer =
      DecoderBuffer::CopyFrom(buffer.data(), size);
  input_buffer->set_timestamp(pts);
  input_buffer->set_is_key_frame(key_frame);

  // Sends frame out if there is pending read callback. Otherwise, stores it
  // in the buffer queue.
  if (!pending_read_cb_) {
    buffer_queue_.push_back(input_buffer);
  } else {
    std::move(pending_read_cb_).Run(kOk, input_buffer);
  }
}

FakeMediaResource::FakeMediaResource()
    : demuxer_stream_(new FakeDemuxerStream(true)) {}

FakeMediaResource::~FakeMediaResource() = default;

std::vector<DemuxerStream*> FakeMediaResource::GetAllStreams() {
  std::vector<DemuxerStream*> streams;
  streams.push_back(demuxer_stream_.get());
  return streams;
}

}  // namespace remoting
}  // namespace media
