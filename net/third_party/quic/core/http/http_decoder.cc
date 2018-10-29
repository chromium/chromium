// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/http_decoder.h"
#include "net/third_party/quic/core/quic_data_reader.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_fallthrough.h"

namespace quic {

namespace {

// Create a mask that sets the last |num_bits| to 1 and the rest to 0.
inline uint8_t GetMaskFromNumBits(uint8_t num_bits) {
  return (1u << num_bits) - 1;
}

// Extract |num_bits| from |flags| offset by |offset|.
uint8_t ExtractBits(uint8_t flags, uint8_t num_bits, uint8_t offset) {
  return (flags >> offset) & GetMaskFromNumBits(num_bits);
}

}  // namespace

HttpDecoder::HttpDecoder()
    : visitor_(nullptr),
      state_(STATE_READING_FRAME_LENGTH),
      current_frame_type_(0),
      current_frame_length_(0),
      remaining_frame_length_(0),
      error_(QUIC_NO_ERROR),
      error_detail_("") {}

HttpDecoder::~HttpDecoder() {}

size_t HttpDecoder::ProcessInput(const char* data, size_t len) {
  QuicDataReader reader(data, len, NETWORK_BYTE_ORDER);
  while (error_ == QUIC_NO_ERROR && reader.BytesRemaining() != 0) {
    switch (state_) {
      case STATE_READING_FRAME_LENGTH:
        ReadFrameLength(&reader);
        break;
      case STATE_READING_FRAME_TYPE:
        ReadFrameType(&reader);
        break;
      case STATE_READING_FRAME_PAYLOAD:
        ReadFramePayload(&reader);
        break;
      case STATE_ERROR:
        break;
      default:
        QUIC_BUG << "Invalid state: " << state_;
    }
  }

  if (error_ != QUIC_NO_ERROR) {
    return 0;
  }

  return len - reader.BytesRemaining();
}

void HttpDecoder::ReadFrameLength(QuicDataReader* reader) {
  DCHECK_NE(0u, reader->BytesRemaining());
  if (!reader->ReadVarInt62(&current_frame_length_)) {
    // TODO(rch): Handle partial delivery.
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read frame length");
    return;
  }

  state_ = STATE_READING_FRAME_TYPE;
  remaining_frame_length_ = current_frame_length_;
}

void HttpDecoder::ReadFrameType(QuicDataReader* reader) {
  DCHECK_NE(0u, reader->BytesRemaining());
  if (!reader->ReadUInt8(&current_frame_type_)) {
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read frame type");
    return;
  }

  state_ = STATE_READING_FRAME_PAYLOAD;
}

void HttpDecoder::ReadFramePayload(QuicDataReader* reader) {
  DCHECK_NE(0u, reader->BytesRemaining());
  switch (current_frame_type_) {
    case 0x0: {  // DATA
      if (current_frame_length_ == remaining_frame_length_) {
        visitor_->OnDataFrameStart();
      }
      size_t bytes_to_read =
          std::min<size_t>(remaining_frame_length_, reader->BytesRemaining());
      QuicStringPiece payload;
      if (!reader->ReadStringPiece(&payload, bytes_to_read)) {
        RaiseError(QUIC_INTERNAL_ERROR, "Unable to read data");
        return;
      }
      visitor_->OnDataFramePayload(payload);
      remaining_frame_length_ -= payload.length();
      if (remaining_frame_length_ == 0) {
        state_ = STATE_READING_FRAME_LENGTH;
        visitor_->OnDataFrameEnd();
      }
      return;
    }
    case 0x1: {  // HEADERS
      if (current_frame_length_ == remaining_frame_length_) {
        visitor_->OnHeadersFrameStart();
      }
      size_t bytes_to_read =
          std::min<size_t>(remaining_frame_length_, reader->BytesRemaining());
      QuicStringPiece payload;
      if (!reader->ReadStringPiece(&payload, bytes_to_read)) {
        RaiseError(QUIC_INTERNAL_ERROR, "Unable to read data");
        return;
      }
      visitor_->OnHeadersFramePayload(payload);
      remaining_frame_length_ -= payload.length();
      if (remaining_frame_length_ == 0) {
        state_ = STATE_READING_FRAME_LENGTH;
        visitor_->OnHeadersFrameEnd();
      }
      return;
    }
    case 0x2: {  // PRIORITY
      // TODO(rch): avoid buffering if the entire frame is present, and
      // instead parse directly out of |reader|.
      BufferFramePayload(reader);
      if (remaining_frame_length_ == 0) {
        PriorityFrame frame;
        QuicDataReader reader(buffer_.data(), current_frame_length_,
                              NETWORK_BYTE_ORDER);
        if (!ParsePriorityFrame(&reader, &frame)) {
          return;
        }
        visitor_->OnPriorityFrame(frame);
        state_ = STATE_READING_FRAME_LENGTH;
      }
      return;
    }
    case 0x3: {  // CANCEL_PUSH
      // TODO(rch): Handle partial delivery.
      BufferFramePayload(reader);
      if (remaining_frame_length_ == 0) {
        CancelPushFrame frame;
        QuicDataReader reader(buffer_.data(), current_frame_length_,
                              NETWORK_BYTE_ORDER);
        if (!reader.ReadVarInt62(&frame.push_id)) {
          RaiseError(QUIC_INTERNAL_ERROR, "Unable to read push_id");
          return;
        }
        visitor_->OnCancelPushFrame(frame);
        state_ = STATE_READING_FRAME_LENGTH;
      }
      return;
    }
    case 0x4: {  // SETTINGS
      // TODO(rch): Handle overly large SETTINGS frames. Either:
      // 1. Impose a limit on SETTINGS frame size, and close the connection if
      //    exceeded
      // 2. Implement a streaming parsing mode.
      BufferFramePayload(reader);
      if (remaining_frame_length_ == 0) {
        SettingsFrame frame;
        QuicDataReader reader(buffer_.data(), current_frame_length_,
                              NETWORK_BYTE_ORDER);
        if (!ParseSettingsFrame(&reader, &frame)) {
          return;
        }
        visitor_->OnSettingsFrame(frame);
        state_ = STATE_READING_FRAME_LENGTH;
      }
      return;
    }
    case 0x5: {  // PUSH_PROMISE
      if (current_frame_length_ == remaining_frame_length_) {
        size_t bytes_remaining = reader->BytesRemaining();
        PushId push_id;
        // TODO(rch): Handle partial delivery of this field.
        if (!reader->ReadVarInt62(&push_id)) {
          RaiseError(QUIC_INTERNAL_ERROR, "Unable to read push_id");
          return;
        }
        remaining_frame_length_ -= bytes_remaining - reader->BytesRemaining();
        visitor_->OnPushPromiseFrameStart(push_id);
      }
      size_t bytes_to_read =
          std::min<size_t>(remaining_frame_length_, reader->BytesRemaining());
      if (bytes_to_read == 0) {
        return;
      }
      QuicStringPiece payload;
      if (!reader->ReadStringPiece(&payload, bytes_to_read)) {
        RaiseError(QUIC_INTERNAL_ERROR, "Unable to read data");
        return;
      }
      visitor_->OnPushPromiseFramePayload(payload);
      remaining_frame_length_ -= payload.length();
      if (remaining_frame_length_ == 0) {
        state_ = STATE_READING_FRAME_LENGTH;
        visitor_->OnPushPromiseFrameEnd();
      }
      return;
    }
    case 0x7: {  // GOAWAY
      BufferFramePayload(reader);
      if (remaining_frame_length_ == 0) {
        GoAwayFrame frame;
        QuicDataReader reader(buffer_.data(), current_frame_length_,
                              NETWORK_BYTE_ORDER);
        uint64_t stream_id;
        if (!reader.ReadVarInt62(&stream_id)) {
          RaiseError(QUIC_INTERNAL_ERROR, "Unable to read GOAWAY stream_id");
          return;
        }
        frame.stream_id = stream_id;
        visitor_->OnGoAwayFrame(frame);
        state_ = STATE_READING_FRAME_LENGTH;
      }
      return;
    }

    case 0xD: {  // MAX_PUSH_ID
      // TODO(rch): Handle partial delivery.
      BufferFramePayload(reader);
      if (remaining_frame_length_ == 0) {
        QuicDataReader reader(buffer_.data(), current_frame_length_,
                              NETWORK_BYTE_ORDER);
        MaxPushIdFrame frame;
        if (!reader.ReadVarInt62(&frame.push_id)) {
          RaiseError(QUIC_INTERNAL_ERROR, "Unable to read push_id");
          return;
        }
        visitor_->OnMaxPushIdFrame(frame);
        state_ = STATE_READING_FRAME_LENGTH;
      }
      return;
    }
    // Reserved frame types.
    // TODO(rch): Since these are actually the same behavior as the
    // default, we probably don't need to special case them here?
    case 0xB:
      QUIC_FALLTHROUGH_INTENDED;
    case 0xB + 0x1F:
      QUIC_FALLTHROUGH_INTENDED;
    case 0xB + 0x1F * 2:
      QUIC_FALLTHROUGH_INTENDED;
    case 0xB + 0x1F * 3:
      QUIC_FALLTHROUGH_INTENDED;
    case 0xB + 0x1F * 4:
      QUIC_FALLTHROUGH_INTENDED;
    case 0xB + 0x1F * 5:
      QUIC_FALLTHROUGH_INTENDED;
    case 0xB + 0x1F * 6:
      QUIC_FALLTHROUGH_INTENDED;
    case 0xB + 0x1F * 7:
      QUIC_FALLTHROUGH_INTENDED;
    default:
      DiscardFramePayload(reader);
  }
}

void HttpDecoder::DiscardFramePayload(QuicDataReader* reader) {
  size_t bytes_to_read =
      std::min<size_t>(remaining_frame_length_, reader->BytesRemaining());
  QuicStringPiece payload;
  if (!reader->ReadStringPiece(&payload, bytes_to_read)) {
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read frame payload");
    return;
  }
  remaining_frame_length_ -= payload.length();
  if (remaining_frame_length_ == 0) {
    state_ = STATE_READING_FRAME_LENGTH;
  }
}

void HttpDecoder::BufferFramePayload(QuicDataReader* reader) {
  if (current_frame_length_ == remaining_frame_length_) {
    buffer_.erase(buffer_.size());
    buffer_.reserve(current_frame_length_);
  }
  size_t bytes_to_read =
      std::min<size_t>(remaining_frame_length_, reader->BytesRemaining());
  if (!reader->ReadBytes(
          &(buffer_[0]) + current_frame_length_ - remaining_frame_length_,
          bytes_to_read)) {
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read frame payload");
    return;
  }
  remaining_frame_length_ -= bytes_to_read;
}

void HttpDecoder::RaiseError(QuicErrorCode error, QuicString error_detail) {
  state_ = STATE_ERROR;
  error_ = error;
  error_detail_ = std::move(error_detail);
}

bool HttpDecoder::ParsePriorityFrame(QuicDataReader* reader,
                                     PriorityFrame* frame) {
  uint8_t flags;
  if (!reader->ReadUInt8(&flags)) {
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read priority frame flags");
    return false;
  }

  frame->prioritized_type =
      static_cast<PriorityElementType>(ExtractBits(flags, 2, 6));
  frame->dependency_type =
      static_cast<PriorityElementType>(ExtractBits(flags, 2, 4));
  frame->exclusive = flags % 2 == 1;
  if (!reader->ReadVarInt62(&frame->prioritized_element_id)) {
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read prioritized_element_id");
    return false;
  }
  if (!reader->ReadVarInt62(&frame->element_dependency_id)) {
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read element_dependency_id");
    return false;
  }
  if (!reader->ReadUInt8(&frame->weight)) {
    RaiseError(QUIC_INTERNAL_ERROR, "Unable to read priority frame weight");
    return false;
  }
  return true;
}

bool HttpDecoder::ParseSettingsFrame(QuicDataReader* reader,
                                     SettingsFrame* frame) {
  while (!reader->IsDoneReading()) {
    uint16_t id;
    if (!reader->ReadUInt16(&id)) {
      RaiseError(QUIC_INTERNAL_ERROR,
                 "Unable to read settings frame identifier");
      return false;
    }
    uint64_t content;
    if (!reader->ReadVarInt62(&content)) {
      RaiseError(QUIC_INTERNAL_ERROR, "Unable to read settings frame content");
      return false;
    }
    frame->values[id] = content;
  }
  return true;
}

}  // namespace quic
