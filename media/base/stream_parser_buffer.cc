// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_parser_buffer.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "media/base/timestamp_constants.h"

namespace media {

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(new StreamParserBuffer(
      NULL, 0, NULL, 0, false, DemuxerStream::UNKNOWN, 0));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8_t* data,
    int data_size,
    bool is_key_frame,
    Type type,
    TrackId track_id) {
  return base::WrapRefCounted(new StreamParserBuffer(
      data, data_size, NULL, 0, is_key_frame, type, track_id));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8_t* data,
    int data_size,
    const uint8_t* side_data,
    int side_data_size,
    bool is_key_frame,
    Type type,
    TrackId track_id) {
  return base::WrapRefCounted(
      new StreamParserBuffer(data, data_size, side_data, side_data_size,
                             is_key_frame, type, track_id));
}

DecodeTimestamp StreamParserBuffer::GetDecodeTimestamp() const {
  if (decode_timestamp_ == kNoDecodeTimestamp())
    return DecodeTimestamp::FromPresentationTime(timestamp());
  return decode_timestamp_;
}

void StreamParserBuffer::SetDecodeTimestamp(DecodeTimestamp timestamp) {
  decode_timestamp_ = timestamp;
  if (preroll_buffer_)
    preroll_buffer_->SetDecodeTimestamp(timestamp);
}

StreamParserBuffer::StreamParserBuffer(const uint8_t* data,
                                       int data_size,
                                       const uint8_t* side_data,
                                       int side_data_size,
                                       bool is_key_frame,
                                       Type type,
                                       TrackId track_id)
    : DecoderBuffer(data, data_size, side_data, side_data_size),
      decode_timestamp_(kNoDecodeTimestamp()),
      config_id_(kInvalidConfigId),
      type_(type),
      track_id_(track_id),
      is_duration_estimated_(false) {
  // TODO(scherkus): Should DataBuffer constructor accept a timestamp and
  // duration to force clients to set them? Today they end up being zero which
  // is both a common and valid value and could lead to bugs.
  if (data) {
    set_duration(kNoTimestamp);
  }

  if (is_key_frame)
    set_is_key_frame(true);
}

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

}  // namespace media
