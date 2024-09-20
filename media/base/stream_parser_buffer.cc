// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_parser_buffer.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/media_client.h"
#include "media/base/timestamp_constants.h"

namespace media {

static_assert(StreamParserBuffer::Type::TYPE_MAX < 4,
              "StreamParserBuffer::type_ has a max storage size of two bits.");

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CreateEOSBuffer(
    std::optional<ConfigVariant> next_config) {
  return base::WrapRefCounted(new StreamParserBuffer(
      DecoderBufferType::kEndOfStream, std::move(next_config)));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8_t* data,
    int data_size,
    bool is_key_frame,
    Type type,
    TrackId track_id) {
  if (auto* media_client = GetMediaClient()) {
    if (auto* alloc = media_client->GetMediaAllocator()) {
      auto data_span =
          UNSAFE_TODO(base::span(data, base::checked_cast<size_t>(data_size)));
      return StreamParserBuffer::FromExternalMemory(
          alloc->CopyFrom(data_span), is_key_frame, type, track_id);
    }
  }
  return base::WrapRefCounted(
      new StreamParserBuffer(data, data_size, is_key_frame, type, track_id));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::FromExternalMemory(
    std::unique_ptr<ExternalMemory> external_memory,
    bool is_key_frame,
    Type type,
    TrackId track_id) {
  return base::WrapRefCounted(new StreamParserBuffer(
      std::move(external_memory), is_key_frame, type, track_id));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::FromArray(
    base::HeapArray<uint8_t> heap_array,
    bool is_key_frame,
    Type type,
    TrackId track_id) {
  return base::WrapRefCounted(new StreamParserBuffer(
      std::move(heap_array), is_key_frame, type, track_id));
}

DecodeTimestamp StreamParserBuffer::GetDecodeTimestamp() const {
  if (decode_timestamp_ == kNoDecodeTimestamp)
    return DecodeTimestamp::FromPresentationTime(timestamp());
  return decode_timestamp_;
}

void StreamParserBuffer::SetDecodeTimestamp(DecodeTimestamp timestamp) {
  decode_timestamp_ = timestamp;
  if (preroll_buffer_)
    preroll_buffer_->SetDecodeTimestamp(timestamp);
}

StreamParserBuffer::StreamParserBuffer(
    std::unique_ptr<ExternalMemory> external_memory,
    bool is_key_frame,
    Type type,
    TrackId track_id)
    : DecoderBuffer(std::move(external_memory)),
      type_(type),
      track_id_(track_id) {
  set_duration(kNoTimestamp);
  set_is_key_frame(is_key_frame);
}

StreamParserBuffer::StreamParserBuffer(base::HeapArray<uint8_t> heap_array,
                                       bool is_key_frame,
                                       Type type,
                                       TrackId track_id)
    : DecoderBuffer(std::move(heap_array)), type_(type), track_id_(track_id) {
  set_duration(kNoTimestamp);
  set_is_key_frame(is_key_frame);
}

StreamParserBuffer::StreamParserBuffer(const uint8_t* data,
                                       int data_size,
                                       bool is_key_frame,
                                       Type type,
                                       TrackId track_id)
    : DecoderBuffer(
          // TODO(crbug.com/40284755): Convert `StreamBufferParser` to
          // `size_t` and `base::span`.
          UNSAFE_TODO(base::span(data, base::checked_cast<size_t>(data_size)))),
      type_(type),
      track_id_(track_id) {
  // TODO(scherkus): Should DataBuffer constructor accept a timestamp and
  // duration to force clients to set them? Today they end up being zero which
  // is both a common and valid value and could lead to bugs.
  if (data) {
    set_duration(kNoTimestamp);
  }

  if (is_key_frame)
    set_is_key_frame(true);
}

StreamParserBuffer::StreamParserBuffer(DecoderBufferType decoder_buffer_type,
                                       std::optional<ConfigVariant> next_config)
    : DecoderBuffer(decoder_buffer_type, next_config),
      type_(Type::UNKNOWN),
      track_id_(-1) {}

StreamParserBuffer::~StreamParserBuffer() = default;

int StreamParserBuffer::GetConfigId() const {
  return config_id_;
}

void StreamParserBuffer::SetConfigId(int config_id) {
  config_id_ = config_id;
  if (preroll_buffer_)
    preroll_buffer_->SetConfigId(config_id);
}

const char* StreamParserBuffer::GetTypeName() const {
  return DemuxerStream::GetTypeName(type());
}

void StreamParserBuffer::SetPrerollBuffer(
    scoped_refptr<StreamParserBuffer> preroll_buffer) {
  DCHECK(!preroll_buffer_);
  DCHECK(!end_of_stream());
  DCHECK(!preroll_buffer->end_of_stream());
  DCHECK(!preroll_buffer->preroll_buffer_);
  DCHECK(preroll_buffer->timestamp() <= timestamp());
  DCHECK(preroll_buffer->discard_padding() == DecoderBuffer::DiscardPadding());
  DCHECK_EQ(preroll_buffer->type(), type());
  DCHECK_EQ(preroll_buffer->track_id(), track_id());

  preroll_buffer_ = std::move(preroll_buffer);
  preroll_buffer_->set_timestamp(timestamp());
  preroll_buffer_->SetConfigId(GetConfigId());
  preroll_buffer_->SetDecodeTimestamp(GetDecodeTimestamp());

  // Mark the entire buffer for discard.
  preroll_buffer_->set_discard_padding(
      std::make_pair(kInfiniteDuration, base::TimeDelta()));
}

void StreamParserBuffer::set_timestamp(base::TimeDelta timestamp) {
  DecoderBuffer::set_timestamp(timestamp);
  if (preroll_buffer_)
    preroll_buffer_->set_timestamp(timestamp);
}

size_t StreamParserBuffer::GetMemoryUsage() const {
  size_t memory_usage = DecoderBuffer::GetMemoryUsage() -
                        sizeof(DecoderBuffer) + sizeof(StreamParserBuffer);

  if (preroll_buffer_) {
    memory_usage += preroll_buffer_->GetMemoryUsage();
  }

  return memory_usage;
}

}  // namespace media
