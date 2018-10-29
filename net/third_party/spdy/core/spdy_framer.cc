// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/spdy_framer.h"

#include <algorithm>
#include <cctype>
#include <ios>
#include <iterator>
#include <list>
#include <new>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "net/third_party/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/spdy/core/spdy_bitmasks.h"
#include "net/third_party/spdy/core/spdy_bug_tracker.h"
#include "net/third_party/spdy/core/spdy_frame_builder.h"
#include "net/third_party/spdy/core/spdy_frame_reader.h"
#include "net/third_party/spdy/platform/api/spdy_estimate_memory_usage.h"
#include "net/third_party/spdy/platform/api/spdy_ptr_util.h"
#include "net/third_party/spdy/platform/api/spdy_string_utils.h"

namespace spdy {

namespace {

// Pack parent stream ID and exclusive flag into the format used by HTTP/2
// headers and priority frames.
uint32_t PackStreamDependencyValues(bool exclusive,
                                    SpdyStreamId parent_stream_id) {
  // Make sure the highest-order bit in the parent stream id is zeroed out.
  uint32_t parent = parent_stream_id & 0x7fffffff;
  // Set the one-bit exclusivity flag.
  uint32_t e_bit = exclusive ? 0x80000000 : 0;
  return parent | e_bit;
}

// Used to indicate no flags in a HTTP2 flags field.
const uint8_t kNoFlags = 0;

// Wire size of pad length field.
const size_t kPadLengthFieldSize = 1;

// The size of one parameter in SETTINGS frame.
const size_t kOneSettingParameterSize = 6;

size_t GetUncompressedSerializedLength(const SpdyHeaderBlock& headers) {
  const size_t num_name_value_pairs_size = sizeof(uint32_t);
  const size_t length_of_name_size = num_name_value_pairs_size;
  const size_t length_of_value_size = num_name_value_pairs_size;

  size_t total_length = num_name_value_pairs_size;
  for (const auto& header : headers) {
    // We add space for the length of the name and the length of the value as
    // well as the length of the name and the length of the value.
    total_length += length_of_name_size + header.first.size() +
                    length_of_value_size + header.second.size();
  }
  return total_length;
}

// Serializes the flags octet for a given SpdyHeadersIR.
uint8_t SerializeHeaderFrameFlags(const SpdyHeadersIR& header_ir,
                                  const bool end_headers) {
  uint8_t flags = 0;
  if (header_ir.fin()) {
    flags |= CONTROL_FLAG_FIN;
  }
  if (end_headers) {
    flags |= HEADERS_FLAG_END_HEADERS;
  }
  if (header_ir.padded()) {
    flags |= HEADERS_FLAG_PADDED;
  }
  if (header_ir.has_priority()) {
    flags |= HEADERS_FLAG_PRIORITY;
  }
  return flags;
}

// Serializes the flags octet for a given SpdyPushPromiseIR.
uint8_t SerializePushPromiseFrameFlags(const SpdyPushPromiseIR& push_promise_ir,
                                       const bool end_headers) {
  uint8_t flags = 0;
  if (push_promise_ir.padded()) {
    flags = flags | PUSH_PROMISE_FLAG_PADDED;
  }
  if (end_headers) {
    flags |= PUSH_PROMISE_FLAG_END_PUSH_PROMISE;
  }
  return flags;
}

// Serializes a HEADERS frame from the given SpdyHeadersIR and encoded header
// block. Does not need or use the SpdyHeaderBlock inside SpdyHeadersIR.
// Return false if the serialization fails. |encoding| should not be empty.
bool SerializeHeadersGivenEncoding(const SpdyHeadersIR& headers,
                                   const SpdyString& encoding,
                                   const bool end_headers,
                                   ZeroCopyOutputBuffer* output) {
  const size_t frame_size =
      GetHeaderFrameSizeSansBlock(headers) + encoding.size();
  SpdyFrameBuilder builder(frame_size, output);
  bool ret = builder.BeginNewFrame(
      SpdyFrameType::HEADERS, SerializeHeaderFrameFlags(headers, end_headers),
      headers.stream_id(), frame_size - kFrameHeaderSize);
  DCHECK_EQ(kFrameHeaderSize, builder.length());

  if (ret && headers.padded()) {
    ret &= builder.WriteUInt8(headers.padding_payload_len());
  }

  if (ret && headers.has_priority()) {
    int weight = ClampHttp2Weight(headers.weight());
    ret &= builder.WriteUInt32(PackStreamDependencyValues(
        headers.exclusive(), headers.parent_stream_id()));
    // Per RFC 7540 section 6.3, serialized weight value is actual value - 1.
    ret &= builder.WriteUInt8(weight - 1);
  }

  if (ret) {
    ret &= builder.WriteBytes(encoding.data(), encoding.size());
  }

  if (ret && headers.padding_payload_len() > 0) {
    SpdyString padding(headers.padding_payload_len(), 0);
    ret &= builder.WriteBytes(padding.data(), padding.length());
  }

  if (!ret) {
    DLOG(WARNING) << "Failed to build HEADERS. Not enough space in output";
  }
  return ret;
}

// Serializes a PUSH_PROMISE frame from the given SpdyPushPromiseIR and
// encoded header block. Does not need or use the SpdyHeaderBlock inside
// SpdyPushPromiseIR.
bool SerializePushPromiseGivenEncoding(const SpdyPushPromiseIR& push_promise,
                                       const SpdyString& encoding,
                                       const bool end_headers,
                                       ZeroCopyOutputBuffer* output) {
  const size_t frame_size =
      GetPushPromiseFrameSizeSansBlock(push_promise) + encoding.size();
  SpdyFrameBuilder builder(frame_size, output);
  bool ok = builder.BeginNewFrame(
      SpdyFrameType::PUSH_PROMISE,
      SerializePushPromiseFrameFlags(push_promise, end_headers),
      push_promise.stream_id(), frame_size - kFrameHeaderSize);

  if (push_promise.padded()) {
    ok = ok && builder.WriteUInt8(push_promise.padding_payload_len());
  }
  ok = ok && builder.WriteUInt32(push_promise.promised_stream_id()) &&
       builder.WriteBytes(encoding.data(), encoding.size());
  if (ok && push_promise.padding_payload_len() > 0) {
    SpdyString padding(push_promise.padding_payload_len(), 0);
    ok = builder.WriteBytes(padding.data(), padding.length());
  }

  DLOG_IF(ERROR, !ok) << "Failed to write PUSH_PROMISE encoding, not enough "
                      << "space in output";
  return ok;
}

bool WritePayloadWithContinuation(SpdyFrameBuilder* builder,
                                  const SpdyString& hpack_encoding,
                                  SpdyStreamId stream_id,
                                  SpdyFrameType type,
                                  int padding_payload_len) {
  uint8_t end_flag = 0;
  uint8_t flags = 0;
  if (type == SpdyFrameType::HEADERS) {
    end_flag = HEADERS_FLAG_END_HEADERS;
  } else if (type == SpdyFrameType::PUSH_PROMISE) {
    end_flag = PUSH_PROMISE_FLAG_END_PUSH_PROMISE;
  } else {
    DLOG(FATAL) << "CONTINUATION frames cannot be used with frame type "
                << FrameTypeToString(type);
  }

  // Write all the padding payload and as much of the data payload as possible
  // into the initial frame.
  size_t bytes_remaining = 0;
  bytes_remaining = hpack_encoding.size() -
                    std::min(hpack_encoding.size(),
                             kHttp2MaxControlFrameSendSize - builder->length() -
                                 padding_payload_len);
  bool ret = builder->WriteBytes(&hpack_encoding[0],
                                 hpack_encoding.size() - bytes_remaining);
  if (padding_payload_len > 0) {
    SpdyString padding = SpdyString(padding_payload_len, 0);
    ret &= builder->WriteBytes(padding.data(), padding.length());
  }

  // Tack on CONTINUATION frames for the overflow.
  while (bytes_remaining > 0 && ret) {
    size_t bytes_to_write =
        std::min(bytes_remaining,
                 kHttp2MaxControlFrameSendSize - kContinuationFrameMinimumSize);
    // Write CONTINUATION frame prefix.
    if (bytes_remaining == bytes_to_write) {
      flags |= end_flag;
    }
    ret &= builder->BeginNewFrame(SpdyFrameType::CONTINUATION, flags, stream_id,
                                  bytes_to_write);
    // Write payload fragment.
    ret &= builder->WriteBytes(
        &hpack_encoding[hpack_encoding.size() - bytes_remaining],
        bytes_to_write);
    bytes_remaining -= bytes_to_write;
  }
  return ret;
}

void SerializeDataBuilderHelper(const SpdyDataIR& data_ir,
                                uint8_t* flags,
                                int* num_padding_fields,
                                size_t* size_with_padding) {
  if (data_ir.fin()) {
    *flags = DATA_FLAG_FIN;
  }

  if (data_ir.padded()) {
    *flags = *flags | DATA_FLAG_PADDED;
    ++*num_padding_fields;
  }

  *size_with_padding = *num_padding_fields + data_ir.data_len() +
                       data_ir.padding_payload_len() + kDataFrameMinimumSize;
}

void SerializeDataFrameHeaderWithPaddingLengthFieldBuilderHelper(
    const SpdyDataIR& data_ir,
    uint8_t* flags,
    size_t* frame_size,
    size_t* num_padding_fields) {
  *flags = DATA_FLAG_NONE;
  if (data_ir.fin()) {
    *flags = DATA_FLAG_FIN;
  }

  *frame_size = kDataFrameMinimumSize;
  if (data_ir.padded()) {
    *flags = *flags | DATA_FLAG_PADDED;
    ++(*num_padding_fields);
    *frame_size = *frame_size + *num_padding_fields;
  }
}

void SerializeSettingsBuilderHelper(const SpdySettingsIR& settings,
                                    uint8_t* flags,
                                    const SettingsMap* values,
                                    size_t* size) {
  if (settings.is_ack()) {
    *flags = *flags | SETTINGS_FLAG_ACK;
  }
  *size =
      kSettingsFrameMinimumSize + (values->size() * kOneSettingParameterSize);
}

void SerializeAltSvcBuilderHelper(const SpdyAltSvcIR& altsvc_ir,
                                  SpdyString* value,
                                  size_t* size) {
  *size = kGetAltSvcFrameMinimumSize;
  *size = *size + altsvc_ir.origin().length();
  *value = SpdyAltSvcWireFormat::SerializeHeaderFieldValue(
      altsvc_ir.altsvc_vector());
  *size = *size + value->length();
}

}  // namespace

SpdyFramer::SpdyFramer(CompressionOption option)
    : debug_visitor_(nullptr), compression_option_(option) {
  static_assert(kHttp2MaxControlFrameSendSize <= kHttp2DefaultFrameSizeLimit,
                "Our send limit should be at most our receive limit.");
}

SpdyFramer::~SpdyFramer() = default;

void SpdyFramer::set_debug_visitor(
    SpdyFramerDebugVisitorInterface* debug_visitor) {
  debug_visitor_ = debug_visitor;
}

SpdyFramer::SpdyFrameIterator::SpdyFrameIterator(SpdyFramer* framer)
    : framer_(framer), is_first_frame_(true), has_next_frame_(true) {}

SpdyFramer::SpdyFrameIterator::~SpdyFrameIterator() = default;

size_t SpdyFramer::SpdyFrameIterator::NextFrame(ZeroCopyOutputBuffer* output) {
  const SpdyFrameIR& frame_ir = GetIR();
  if (!has_next_frame_) {
    SPDY_BUG << "SpdyFramer::SpdyFrameIterator::NextFrame called without "
             << "a next frame.";
    return false;
  }

  const size_t size_without_block =
      is_first_frame_ ? GetFrameSizeSansBlock() : kContinuationFrameMinimumSize;
  auto encoding = SpdyMakeUnique<SpdyString>();
  encoder_->Next(kHttp2MaxControlFrameSendSize - size_without_block,
                 encoding.get());
  has_next_frame_ = encoder_->HasNext();

  if (framer_->debug_visitor_ != nullptr) {
    const auto& header_block_frame_ir =
        static_cast<const SpdyFrameWithHeaderBlockIR&>(frame_ir);
    const size_t header_list_size =
        GetUncompressedSerializedLength(header_block_frame_ir.header_block());
    framer_->debug_visitor_->OnSendCompressedFrame(
        frame_ir.stream_id(),
        is_first_frame_ ? frame_ir.frame_type() : SpdyFrameType::CONTINUATION,
        header_list_size, size_without_block + encoding->size());
  }

  const size_t free_bytes_before = output->BytesFree();
  bool ok = false;
  if (is_first_frame_) {
    is_first_frame_ = false;
    ok = SerializeGivenEncoding(*encoding, output);
  } else {
    SpdyContinuationIR continuation_ir(frame_ir.stream_id());
    continuation_ir.take_encoding(std::move(encoding));
    continuation_ir.set_end_headers(!has_next_frame_);
    ok = framer_->SerializeContinuation(continuation_ir, output);
  }
  return ok ? free_bytes_before - output->BytesFree() : 0;
}

bool SpdyFramer::SpdyFrameIterator::HasNextFrame() const {
  return has_next_frame_;
}

SpdyFramer::SpdyHeaderFrameIterator::SpdyHeaderFrameIterator(
    SpdyFramer* framer,
    std::unique_ptr<const SpdyHeadersIR> headers_ir)
    : SpdyFrameIterator(framer), headers_ir_(std::move(headers_ir)) {
  SetEncoder(headers_ir_.get());
}

SpdyFramer::SpdyHeaderFrameIterator::~SpdyHeaderFrameIterator() = default;

const SpdyFrameIR& SpdyFramer::SpdyHeaderFrameIterator::GetIR() const {
  return *(headers_ir_.get());
}

size_t SpdyFramer::SpdyHeaderFrameIterator::GetFrameSizeSansBlock() const {
  return GetHeaderFrameSizeSansBlock(*headers_ir_);
}

bool SpdyFramer::SpdyHeaderFrameIterator::SerializeGivenEncoding(
    const SpdyString& encoding,
    ZeroCopyOutputBuffer* output) const {
  return SerializeHeadersGivenEncoding(*headers_ir_, encoding,
                                       !has_next_frame(), output);
}

SpdyFramer::SpdyPushPromiseFrameIterator::SpdyPushPromiseFrameIterator(
    SpdyFramer* framer,
    std::unique_ptr<const SpdyPushPromiseIR> push_promise_ir)
    : SpdyFrameIterator(framer), push_promise_ir_(std::move(push_promise_ir)) {
  SetEncoder(push_promise_ir_.get());
}

SpdyFramer::SpdyPushPromiseFrameIterator::~SpdyPushPromiseFrameIterator() =
    default;

const SpdyFrameIR& SpdyFramer::SpdyPushPromiseFrameIterator::GetIR() const {
  return *(push_promise_ir_.get());
}

size_t SpdyFramer::SpdyPushPromiseFrameIterator::GetFrameSizeSansBlock() const {
  return GetPushPromiseFrameSizeSansBlock(*push_promise_ir_);
}

bool SpdyFramer::SpdyPushPromiseFrameIterator::SerializeGivenEncoding(
    const SpdyString& encoding,
    ZeroCopyOutputBuffer* output) const {
  return SerializePushPromiseGivenEncoding(*push_promise_ir_, encoding,
                                           !has_next_frame(), output);
}

SpdyFramer::SpdyControlFrameIterator::SpdyControlFrameIterator(
    SpdyFramer* framer,
    std::unique_ptr<const SpdyFrameIR> frame_ir)
    : framer_(framer), frame_ir_(std::move(frame_ir)) {}

SpdyFramer::SpdyControlFrameIterator::~SpdyControlFrameIterator() = default;

size_t SpdyFramer::SpdyControlFrameIterator::NextFrame(
    ZeroCopyOutputBuffer* output) {
  size_t size_written = framer_->SerializeFrame(*frame_ir_, output);
  has_next_frame_ = false;
  return size_written;
}

bool SpdyFramer::SpdyControlFrameIterator::HasNextFrame() const {
  return has_next_frame_;
}

const SpdyFrameIR& SpdyFramer::SpdyControlFrameIterator::GetIR() const {
  return *(frame_ir_.get());
}

// TODO(yasong): remove all the static_casts.
std::unique_ptr<SpdyFrameSequence> SpdyFramer::CreateIterator(
    SpdyFramer* framer,
    std::unique_ptr<const SpdyFrameIR> frame_ir) {
  switch (frame_ir->frame_type()) {
    case SpdyFrameType::HEADERS: {
      return SpdyMakeUnique<SpdyHeaderFrameIterator>(
          framer, SpdyWrapUnique(
                      static_cast<const SpdyHeadersIR*>(frame_ir.release())));
    }
    case SpdyFrameType::PUSH_PROMISE: {
      return SpdyMakeUnique<SpdyPushPromiseFrameIterator>(
          framer, SpdyWrapUnique(static_cast<const SpdyPushPromiseIR*>(
                      frame_ir.release())));
    }
    case SpdyFrameType::DATA: {
      DVLOG(1) << "Serialize a stream end DATA frame for VTL";
      FALLTHROUGH;
    }
    default: {
      return SpdyMakeUnique<SpdyControlFrameIterator>(framer,
                                                      std::move(frame_ir));
    }
  }
}

SpdySerializedFrame SpdyFramer::SerializeData(const SpdyDataIR& data_ir) {
  uint8_t flags = DATA_FLAG_NONE;
  int num_padding_fields = 0;
  size_t size_with_padding = 0;
  SerializeDataBuilderHelper(data_ir, &flags, &num_padding_fields,
                             &size_with_padding);

  SpdyFrameBuilder builder(size_with_padding);
  builder.BeginNewFrame(SpdyFrameType::DATA, flags, data_ir.stream_id());
  if (data_ir.padded()) {
    builder.WriteUInt8(data_ir.padding_payload_len() & 0xff);
  }
  builder.WriteBytes(data_ir.data(), data_ir.data_len());
  if (data_ir.padding_payload_len() > 0) {
    SpdyString padding(data_ir.padding_payload_len(), 0);
    builder.WriteBytes(padding.data(), padding.length());
  }
  DCHECK_EQ(size_with_padding, builder.length());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeDataFrameHeaderWithPaddingLengthField(
    const SpdyDataIR& data_ir) {
  uint8_t flags = DATA_FLAG_NONE;
  size_t frame_size = 0;
  size_t num_padding_fields = 0;
  SerializeDataFrameHeaderWithPaddingLengthFieldBuilderHelper(
      data_ir, &flags, &frame_size, &num_padding_fields);

  SpdyFrameBuilder builder(frame_size);
  builder.BeginNewFrame(
      SpdyFrameType::DATA, flags, data_ir.stream_id(),
      num_padding_fields + data_ir.data_len() + data_ir.padding_payload_len());
  if (data_ir.padded()) {
    builder.WriteUInt8(data_ir.padding_payload_len() & 0xff);
  }
  DCHECK_EQ(frame_size, builder.length());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeRstStream(
    const SpdyRstStreamIR& rst_stream) const {
  size_t expected_length = kRstStreamFrameSize;
  SpdyFrameBuilder builder(expected_length);

  builder.BeginNewFrame(SpdyFrameType::RST_STREAM, 0, rst_stream.stream_id());

  builder.WriteUInt32(rst_stream.error_code());

  DCHECK_EQ(expected_length, builder.length());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeSettings(
    const SpdySettingsIR& settings) const {
  uint8_t flags = 0;
  // Size, in bytes, of this SETTINGS frame.
  size_t size = 0;
  const SettingsMap* values = &(settings.values());
  SerializeSettingsBuilderHelper(settings, &flags, values, &size);
  SpdyFrameBuilder builder(size);
  builder.BeginNewFrame(SpdyFrameType::SETTINGS, flags, 0);

  // If this is an ACK, payload should be empty.
  if (settings.is_ack()) {
    return builder.take();
  }

  DCHECK_EQ(kSettingsFrameMinimumSize, builder.length());
  for (auto it = values->begin(); it != values->end(); ++it) {
    int setting_id = it->first;
    DCHECK_GE(setting_id, 0);
    builder.WriteUInt16(static_cast<SpdySettingsId>(setting_id));
    builder.WriteUInt32(it->second);
  }
  DCHECK_EQ(size, builder.length());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializePing(const SpdyPingIR& ping) const {
  SpdyFrameBuilder builder(kPingFrameSize);
  uint8_t flags = 0;
  if (ping.is_ack()) {
    flags |= PING_FLAG_ACK;
  }
  builder.BeginNewFrame(SpdyFrameType::PING, flags, 0);
  builder.WriteUInt64(ping.id());
  DCHECK_EQ(kPingFrameSize, builder.length());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeGoAway(
    const SpdyGoAwayIR& goaway) const {
  // Compute the output buffer size, take opaque data into account.
  size_t expected_length = kGoawayFrameMinimumSize;
  expected_length += goaway.description().size();
  SpdyFrameBuilder builder(expected_length);

  // Serialize the GOAWAY frame.
  builder.BeginNewFrame(SpdyFrameType::GOAWAY, 0, 0);

  // GOAWAY frames specify the last good stream id.
  builder.WriteUInt32(goaway.last_good_stream_id());

  // GOAWAY frames also specify the error code.
  builder.WriteUInt32(goaway.error_code());

  // GOAWAY frames may also specify opaque data.
  if (!goaway.description().empty()) {
    builder.WriteBytes(goaway.description().data(),
                       goaway.description().size());
  }

  DCHECK_EQ(expected_length, builder.length());
  return builder.take();
}

void SpdyFramer::SerializeHeadersBuilderHelper(const SpdyHeadersIR& headers,
                                               uint8_t* flags,
                                               size_t* size,
                                               SpdyString* hpack_encoding,
                                               int* weight,
                                               size_t* length_field) {
  if (headers.fin()) {
    *flags = *flags | CONTROL_FLAG_FIN;
  }
  // This will get overwritten if we overflow into a CONTINUATION frame.
  *flags = *flags | HEADERS_FLAG_END_HEADERS;
  if (headers.has_priority()) {
    *flags = *flags | HEADERS_FLAG_PRIORITY;
  }
  if (headers.padded()) {
    *flags = *flags | HEADERS_FLAG_PADDED;
  }

  *size = kHeadersFrameMinimumSize;

  if (headers.padded()) {
    *size = *size + kPadLengthFieldSize;
    *size = *size + headers.padding_payload_len();
  }

  if (headers.has_priority()) {
    *weight = ClampHttp2Weight(headers.weight());
    *size = *size + 5;
  }

  GetHpackEncoder()->EncodeHeaderSet(headers.header_block(), hpack_encoding);
  *size = *size + hpack_encoding->size();
  if (*size > kHttp2MaxControlFrameSendSize) {
    *size = *size + GetNumberRequiredContinuationFrames(*size) *
                        kContinuationFrameMinimumSize;
    *flags = *flags & ~HEADERS_FLAG_END_HEADERS;
  }
  // Compute frame length field.
  if (headers.padded()) {
    *length_field = *length_field + kPadLengthFieldSize;
  }
  if (headers.has_priority()) {
    *length_field = *length_field + 4;  // Dependency field.
    *length_field = *length_field + 1;  // Weight field.
  }
  *length_field = *length_field + headers.padding_payload_len();
  *length_field = *length_field + hpack_encoding->size();
  // If the HEADERS frame with payload would exceed the max frame size, then
  // WritePayloadWithContinuation() will serialize CONTINUATION frames as
  // necessary.
  *length_field =
      std::min(*length_field, kHttp2MaxControlFrameSendSize - kFrameHeaderSize);
}

SpdySerializedFrame SpdyFramer::SerializeHeaders(const SpdyHeadersIR& headers) {
  uint8_t flags = 0;
  // The size of this frame, including padding (if there is any) and
  // variable-length header block.
  size_t size = 0;
  SpdyString hpack_encoding;
  int weight = 0;
  size_t length_field = 0;
  SerializeHeadersBuilderHelper(headers, &flags, &size, &hpack_encoding,
                                &weight, &length_field);

  SpdyFrameBuilder builder(size);
  builder.BeginNewFrame(SpdyFrameType::HEADERS, flags, headers.stream_id(),
                        length_field);

  DCHECK_EQ(kHeadersFrameMinimumSize, builder.length());

  int padding_payload_len = 0;
  if (headers.padded()) {
    builder.WriteUInt8(headers.padding_payload_len());
    padding_payload_len = headers.padding_payload_len();
  }
  if (headers.has_priority()) {
    builder.WriteUInt32(PackStreamDependencyValues(headers.exclusive(),
                                                   headers.parent_stream_id()));
    // Per RFC 7540 section 6.3, serialized weight value is actual value - 1.
    builder.WriteUInt8(weight - 1);
  }
  WritePayloadWithContinuation(&builder, hpack_encoding, headers.stream_id(),
                               SpdyFrameType::HEADERS, padding_payload_len);

  if (debug_visitor_) {
    const size_t header_list_size =
        GetUncompressedSerializedLength(headers.header_block());
    debug_visitor_->OnSendCompressedFrame(headers.stream_id(),
                                          SpdyFrameType::HEADERS,
                                          header_list_size, builder.length());
  }

  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeWindowUpdate(
    const SpdyWindowUpdateIR& window_update) {
  SpdyFrameBuilder builder(kWindowUpdateFrameSize);
  builder.BeginNewFrame(SpdyFrameType::WINDOW_UPDATE, kNoFlags,
                        window_update.stream_id());
  builder.WriteUInt32(window_update.delta());
  DCHECK_EQ(kWindowUpdateFrameSize, builder.length());
  return builder.take();
}

void SpdyFramer::SerializePushPromiseBuilderHelper(
    const SpdyPushPromiseIR& push_promise,
    uint8_t* flags,
    SpdyString* hpack_encoding,
    size_t* size) {
  *flags = 0;
  // This will get overwritten if we overflow into a CONTINUATION frame.
  *flags = *flags | PUSH_PROMISE_FLAG_END_PUSH_PROMISE;
  // The size of this frame, including variable-length name-value block.
  *size = kPushPromiseFrameMinimumSize;

  if (push_promise.padded()) {
    *flags = *flags | PUSH_PROMISE_FLAG_PADDED;
    *size = *size + kPadLengthFieldSize;
    *size = *size + push_promise.padding_payload_len();
  }

  GetHpackEncoder()->EncodeHeaderSet(push_promise.header_block(),
                                     hpack_encoding);
  *size = *size + hpack_encoding->size();
  if (*size > kHttp2MaxControlFrameSendSize) {
    *size = *size + GetNumberRequiredContinuationFrames(*size) *
                        kContinuationFrameMinimumSize;
    *flags = *flags & ~PUSH_PROMISE_FLAG_END_PUSH_PROMISE;
  }
}

SpdySerializedFrame SpdyFramer::SerializePushPromise(
    const SpdyPushPromiseIR& push_promise) {
  uint8_t flags = 0;
  size_t size = 0;
  SpdyString hpack_encoding;
  SerializePushPromiseBuilderHelper(push_promise, &flags, &hpack_encoding,
                                    &size);

  SpdyFrameBuilder builder(size);
  size_t length =
      std::min(size, kHttp2MaxControlFrameSendSize) - kFrameHeaderSize;
  builder.BeginNewFrame(SpdyFrameType::PUSH_PROMISE, flags,
                        push_promise.stream_id(), length);
  int padding_payload_len = 0;
  if (push_promise.padded()) {
    builder.WriteUInt8(push_promise.padding_payload_len());
    builder.WriteUInt32(push_promise.promised_stream_id());
    DCHECK_EQ(kPushPromiseFrameMinimumSize + kPadLengthFieldSize,
              builder.length());

    padding_payload_len = push_promise.padding_payload_len();
  } else {
    builder.WriteUInt32(push_promise.promised_stream_id());
    DCHECK_EQ(kPushPromiseFrameMinimumSize, builder.length());
  }

  WritePayloadWithContinuation(
      &builder, hpack_encoding, push_promise.stream_id(),
      SpdyFrameType::PUSH_PROMISE, padding_payload_len);

  if (debug_visitor_) {
    const size_t header_list_size =
        GetUncompressedSerializedLength(push_promise.header_block());
    debug_visitor_->OnSendCompressedFrame(push_promise.stream_id(),
                                          SpdyFrameType::PUSH_PROMISE,
                                          header_list_size, builder.length());
  }

  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeContinuation(
    const SpdyContinuationIR& continuation) const {
  const SpdyString& encoding = continuation.encoding();
  size_t frame_size = kContinuationFrameMinimumSize + encoding.size();
  SpdyFrameBuilder builder(frame_size);
  uint8_t flags = continuation.end_headers() ? HEADERS_FLAG_END_HEADERS : 0;
  builder.BeginNewFrame(SpdyFrameType::CONTINUATION, flags,
                        continuation.stream_id());
  DCHECK_EQ(kFrameHeaderSize, builder.length());

  builder.WriteBytes(encoding.data(), encoding.size());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeAltSvc(const SpdyAltSvcIR& altsvc_ir) {
  SpdyString value;
  size_t size = 0;
  SerializeAltSvcBuilderHelper(altsvc_ir, &value, &size);
  SpdyFrameBuilder builder(size);
  builder.BeginNewFrame(SpdyFrameType::ALTSVC, kNoFlags, altsvc_ir.stream_id());

  builder.WriteUInt16(altsvc_ir.origin().length());
  builder.WriteBytes(altsvc_ir.origin().data(), altsvc_ir.origin().length());
  builder.WriteBytes(value.data(), value.length());
  DCHECK_LT(kGetAltSvcFrameMinimumSize, builder.length());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializePriority(
    const SpdyPriorityIR& priority) const {
  SpdyFrameBuilder builder(kPriorityFrameSize);
  builder.BeginNewFrame(SpdyFrameType::PRIORITY, kNoFlags,
                        priority.stream_id());

  builder.WriteUInt32(PackStreamDependencyValues(priority.exclusive(),
                                                 priority.parent_stream_id()));
  // Per RFC 7540 section 6.3, serialized weight value is actual value - 1.
  builder.WriteUInt8(priority.weight() - 1);
  DCHECK_EQ(kPriorityFrameSize, builder.length());
  return builder.take();
}

SpdySerializedFrame SpdyFramer::SerializeUnknown(
    const SpdyUnknownIR& unknown) const {
  const size_t total_size = kFrameHeaderSize + unknown.payload().size();
  SpdyFrameBuilder builder(total_size);
  builder.BeginNewUncheckedFrame(unknown.type(), unknown.flags(),
                                 unknown.stream_id(), unknown.length());
  builder.WriteBytes(unknown.payload().data(), unknown.payload().size());
  return builder.take();
}

namespace {

class FrameSerializationVisitor : public SpdyFrameVisitor {
 public:
  explicit FrameSerializationVisitor(SpdyFramer* framer)
      : framer_(framer), frame_() {}
  ~FrameSerializationVisitor() override = default;

  SpdySerializedFrame ReleaseSerializedFrame() { return std::move(frame_); }

  void VisitData(const SpdyDataIR& data) override {
    frame_ = framer_->SerializeData(data);
  }
  void VisitRstStream(const SpdyRstStreamIR& rst_stream) override {
    frame_ = framer_->SerializeRstStream(rst_stream);
  }
  void VisitSettings(const SpdySettingsIR& settings) override {
    frame_ = framer_->SerializeSettings(settings);
  }
  void VisitPing(const SpdyPingIR& ping) override {
    frame_ = framer_->SerializePing(ping);
  }
  void VisitGoAway(const SpdyGoAwayIR& goaway) override {
    frame_ = framer_->SerializeGoAway(goaway);
  }
  void VisitHeaders(const SpdyHeadersIR& headers) override {
    frame_ = framer_->SerializeHeaders(headers);
  }
  void VisitWindowUpdate(const SpdyWindowUpdateIR& window_update) override {
    frame_ = framer_->SerializeWindowUpdate(window_update);
  }
  void VisitPushPromise(const SpdyPushPromiseIR& push_promise) override {
    frame_ = framer_->SerializePushPromise(push_promise);
  }
  void VisitContinuation(const SpdyContinuationIR& continuation) override {
    frame_ = framer_->SerializeContinuation(continuation);
  }
  void VisitAltSvc(const SpdyAltSvcIR& altsvc) override {
    frame_ = framer_->SerializeAltSvc(altsvc);
  }
  void VisitPriority(const SpdyPriorityIR& priority) override {
    frame_ = framer_->SerializePriority(priority);
  }
  void VisitUnknown(const SpdyUnknownIR& unknown) override {
    frame_ = framer_->SerializeUnknown(unknown);
  }

 private:
  SpdyFramer* framer_;
  SpdySerializedFrame frame_;
};

// TODO(diannahu): Use also in frame serialization.
class FlagsSerializationVisitor : public SpdyFrameVisitor {
 public:
  void VisitData(const SpdyDataIR& data) override {
    flags_ = DATA_FLAG_NONE;
    if (data.fin()) {
      flags_ |= DATA_FLAG_FIN;
    }
    if (data.padded()) {
      flags_ |= DATA_FLAG_PADDED;
    }
  }

  void VisitRstStream(const SpdyRstStreamIR& rst_stream) override {
    flags_ = kNoFlags;
  }

  void VisitSettings(const SpdySettingsIR& settings) override {
    flags_ = kNoFlags;
    if (settings.is_ack()) {
      flags_ |= SETTINGS_FLAG_ACK;
    }
  }

  void VisitPing(const SpdyPingIR& ping) override {
    flags_ = kNoFlags;
    if (ping.is_ack()) {
      flags_ |= PING_FLAG_ACK;
    }
  }

  void VisitGoAway(const SpdyGoAwayIR& goaway) override { flags_ = kNoFlags; }

  // TODO(diannahu): The END_HEADERS flag is incorrect for HEADERS that require
  //     CONTINUATION frames.
  void VisitHeaders(const SpdyHeadersIR& headers) override {
    flags_ = HEADERS_FLAG_END_HEADERS;
    if (headers.fin()) {
      flags_ |= CONTROL_FLAG_FIN;
    }
    if (headers.padded()) {
      flags_ |= HEADERS_FLAG_PADDED;
    }
    if (headers.has_priority()) {
      flags_ |= HEADERS_FLAG_PRIORITY;
    }
  }

  void VisitWindowUpdate(const SpdyWindowUpdateIR& window_update) override {
    flags_ = kNoFlags;
  }

  // TODO(diannahu): The END_PUSH_PROMISE flag is incorrect for PUSH_PROMISEs
  //     that require CONTINUATION frames.
  void VisitPushPromise(const SpdyPushPromiseIR& push_promise) override {
    flags_ = PUSH_PROMISE_FLAG_END_PUSH_PROMISE;
    if (push_promise.padded()) {
      flags_ |= PUSH_PROMISE_FLAG_PADDED;
    }
  }

  // TODO(diannahu): The END_HEADERS flag is incorrect for CONTINUATIONs that
  //     require CONTINUATION frames.
  void VisitContinuation(const SpdyContinuationIR& continuation) override {
    flags_ = HEADERS_FLAG_END_HEADERS;
  }

  void VisitAltSvc(const SpdyAltSvcIR& altsvc) override { flags_ = kNoFlags; }

  void VisitPriority(const SpdyPriorityIR& priority) override {
    flags_ = kNoFlags;
  }

  uint8_t flags() const { return flags_; }

 private:
  uint8_t flags_ = kNoFlags;
};

}  // namespace

SpdySerializedFrame SpdyFramer::SerializeFrame(const SpdyFrameIR& frame) {
  FrameSerializationVisitor visitor(this);
  frame.Visit(&visitor);
  return visitor.ReleaseSerializedFrame();
}

uint8_t SpdyFramer::GetSerializedFlags(const SpdyFrameIR& frame) {
  FlagsSerializationVisitor visitor;
  frame.Visit(&visitor);
  return visitor.flags();
}

bool SpdyFramer::SerializeData(const SpdyDataIR& data_ir,
                               ZeroCopyOutputBuffer* output) const {
  uint8_t flags = DATA_FLAG_NONE;
  int num_padding_fields = 0;
  size_t size_with_padding = 0;
  SerializeDataBuilderHelper(data_ir, &flags, &num_padding_fields,
                             &size_with_padding);
  SpdyFrameBuilder builder(size_with_padding, output);

  bool ok =
      builder.BeginNewFrame(SpdyFrameType::DATA, flags, data_ir.stream_id());

  if (data_ir.padded()) {
    ok = ok && builder.WriteUInt8(data_ir.padding_payload_len() & 0xff);
  }

  ok = ok && builder.WriteBytes(data_ir.data(), data_ir.data_len());
  if (data_ir.padding_payload_len() > 0) {
    SpdyString padding;
    padding = SpdyString(data_ir.padding_payload_len(), 0);
    ok = ok && builder.WriteBytes(padding.data(), padding.length());
  }
  DCHECK_EQ(size_with_padding, builder.length());
  return ok;
}

bool SpdyFramer::SerializeDataFrameHeaderWithPaddingLengthField(
    const SpdyDataIR& data_ir,
    ZeroCopyOutputBuffer* output) const {
  uint8_t flags = DATA_FLAG_NONE;
  size_t frame_size = 0;
  size_t num_padding_fields = 0;
  SerializeDataFrameHeaderWithPaddingLengthFieldBuilderHelper(
      data_ir, &flags, &frame_size, &num_padding_fields);

  SpdyFrameBuilder builder(frame_size, output);
  bool ok = true;
  ok = ok &&
       builder.BeginNewFrame(SpdyFrameType::DATA, flags, data_ir.stream_id(),
                             num_padding_fields + data_ir.data_len() +
                                 data_ir.padding_payload_len());
  if (data_ir.padded()) {
    ok = ok && builder.WriteUInt8(data_ir.padding_payload_len() & 0xff);
  }
  DCHECK_EQ(frame_size, builder.length());
  return ok;
}

bool SpdyFramer::SerializeRstStream(const SpdyRstStreamIR& rst_stream,
                                    ZeroCopyOutputBuffer* output) const {
  size_t expected_length = kRstStreamFrameSize;
  SpdyFrameBuilder builder(expected_length, output);
  bool ok = builder.BeginNewFrame(SpdyFrameType::RST_STREAM, 0,
                                  rst_stream.stream_id());
  ok = ok && builder.WriteUInt32(rst_stream.error_code());

  DCHECK_EQ(expected_length, builder.length());
  return ok;
}

bool SpdyFramer::SerializeSettings(const SpdySettingsIR& settings,
                                   ZeroCopyOutputBuffer* output) const {
  uint8_t flags = 0;
  // Size, in bytes, of this SETTINGS frame.
  size_t size = 0;
  const SettingsMap* values = &(settings.values());
  SerializeSettingsBuilderHelper(settings, &flags, values, &size);
  SpdyFrameBuilder builder(size, output);
  bool ok = builder.BeginNewFrame(SpdyFrameType::SETTINGS, flags, 0);

  // If this is an ACK, payload should be empty.
  if (settings.is_ack()) {
    return ok;
  }

  DCHECK_EQ(kSettingsFrameMinimumSize, builder.length());
  for (auto it = values->begin(); it != values->end(); ++it) {
    int setting_id = it->first;
    DCHECK_GE(setting_id, 0);
    ok = ok && builder.WriteUInt16(static_cast<SpdySettingsId>(setting_id)) &&
         builder.WriteUInt32(it->second);
  }
  DCHECK_EQ(size, builder.length());
  return ok;
}

bool SpdyFramer::SerializePing(const SpdyPingIR& ping,
                               ZeroCopyOutputBuffer* output) const {
  SpdyFrameBuilder builder(kPingFrameSize, output);
  uint8_t flags = 0;
  if (ping.is_ack()) {
    flags |= PING_FLAG_ACK;
  }
  bool ok = builder.BeginNewFrame(SpdyFrameType::PING, flags, 0);
  ok = ok && builder.WriteUInt64(ping.id());
  DCHECK_EQ(kPingFrameSize, builder.length());
  return ok;
}

bool SpdyFramer::SerializeGoAway(const SpdyGoAwayIR& goaway,
                                 ZeroCopyOutputBuffer* output) const {
  // Compute the output buffer size, take opaque data into account.
  size_t expected_length = kGoawayFrameMinimumSize;
  expected_length += goaway.description().size();
  SpdyFrameBuilder builder(expected_length, output);

  // Serialize the GOAWAY frame.
  bool ok = builder.BeginNewFrame(SpdyFrameType::GOAWAY, 0, 0);

  // GOAWAY frames specify the last good stream id.
  ok = ok && builder.WriteUInt32(goaway.last_good_stream_id()) &&
       // GOAWAY frames also specify the error status code.
       builder.WriteUInt32(goaway.error_code());

  // GOAWAY frames may also specify opaque data.
  if (!goaway.description().empty()) {
    ok = ok && builder.WriteBytes(goaway.description().data(),
                                  goaway.description().size());
  }

  DCHECK_EQ(expected_length, builder.length());
  return ok;
}

bool SpdyFramer::SerializeHeaders(const SpdyHeadersIR& headers,
                                  ZeroCopyOutputBuffer* output) {
  uint8_t flags = 0;
  // The size of this frame, including padding (if there is any) and
  // variable-length header block.
  size_t size = 0;
  SpdyString hpack_encoding;
  int weight = 0;
  size_t length_field = 0;
  SerializeHeadersBuilderHelper(headers, &flags, &size, &hpack_encoding,
                                &weight, &length_field);

  bool ok = true;
  SpdyFrameBuilder builder(size, output);
  ok = ok && builder.BeginNewFrame(SpdyFrameType::HEADERS, flags,
                                   headers.stream_id(), length_field);
  DCHECK_EQ(kHeadersFrameMinimumSize, builder.length());

  int padding_payload_len = 0;
  if (headers.padded()) {
    ok = ok && builder.WriteUInt8(headers.padding_payload_len());
    padding_payload_len = headers.padding_payload_len();
  }
  if (headers.has_priority()) {
    ok = ok &&
         builder.WriteUInt32(PackStreamDependencyValues(
             headers.exclusive(), headers.parent_stream_id())) &&
         // Per RFC 7540 section 6.3, serialized weight value is weight - 1.
         builder.WriteUInt8(weight - 1);
  }
  ok = ok && WritePayloadWithContinuation(
                 &builder, hpack_encoding, headers.stream_id(),
                 SpdyFrameType::HEADERS, padding_payload_len);

  if (debug_visitor_) {
    const size_t header_list_size =
        GetUncompressedSerializedLength(headers.header_block());
    debug_visitor_->OnSendCompressedFrame(headers.stream_id(),
                                          SpdyFrameType::HEADERS,
                                          header_list_size, builder.length());
  }

  return ok;
}

bool SpdyFramer::SerializeWindowUpdate(const SpdyWindowUpdateIR& window_update,
                                       ZeroCopyOutputBuffer* output) const {
  SpdyFrameBuilder builder(kWindowUpdateFrameSize, output);
  bool ok = builder.BeginNewFrame(SpdyFrameType::WINDOW_UPDATE, kNoFlags,
                                  window_update.stream_id());
  ok = ok && builder.WriteUInt32(window_update.delta());
  DCHECK_EQ(kWindowUpdateFrameSize, builder.length());
  return ok;
}

bool SpdyFramer::SerializePushPromise(const SpdyPushPromiseIR& push_promise,
                                      ZeroCopyOutputBuffer* output) {
  uint8_t flags = 0;
  size_t size = 0;
  SpdyString hpack_encoding;
  SerializePushPromiseBuilderHelper(push_promise, &flags, &hpack_encoding,
                                    &size);

  bool ok = true;
  SpdyFrameBuilder builder(size, output);
  size_t length =
      std::min(size, kHttp2MaxControlFrameSendSize) - kFrameHeaderSize;
  ok = builder.BeginNewFrame(SpdyFrameType::PUSH_PROMISE, flags,
                             push_promise.stream_id(), length);

  int padding_payload_len = 0;
  if (push_promise.padded()) {
    ok = ok && builder.WriteUInt8(push_promise.padding_payload_len()) &&
         builder.WriteUInt32(push_promise.promised_stream_id());
    DCHECK_EQ(kPushPromiseFrameMinimumSize + kPadLengthFieldSize,
              builder.length());

    padding_payload_len = push_promise.padding_payload_len();
  } else {
    ok = ok && builder.WriteUInt32(push_promise.promised_stream_id());
    DCHECK_EQ(kPushPromiseFrameMinimumSize, builder.length());
  }

  ok = ok && WritePayloadWithContinuation(
                 &builder, hpack_encoding, push_promise.stream_id(),
                 SpdyFrameType::PUSH_PROMISE, padding_payload_len);

  if (debug_visitor_) {
    const size_t header_list_size =
        GetUncompressedSerializedLength(push_promise.header_block());
    debug_visitor_->OnSendCompressedFrame(push_promise.stream_id(),
                                          SpdyFrameType::PUSH_PROMISE,
                                          header_list_size, builder.length());
  }

  return ok;
}

bool SpdyFramer::SerializeContinuation(const SpdyContinuationIR& continuation,
                                       ZeroCopyOutputBuffer* output) const {
  const SpdyString& encoding = continuation.encoding();
  size_t frame_size = kContinuationFrameMinimumSize + encoding.size();
  SpdyFrameBuilder builder(frame_size, output);
  uint8_t flags = continuation.end_headers() ? HEADERS_FLAG_END_HEADERS : 0;
  bool ok = builder.BeginNewFrame(SpdyFrameType::CONTINUATION, flags,
                                  continuation.stream_id(),
                                  frame_size - kFrameHeaderSize);
  DCHECK_EQ(kFrameHeaderSize, builder.length());

  ok = ok && builder.WriteBytes(encoding.data(), encoding.size());
  return ok;
}

bool SpdyFramer::SerializeAltSvc(const SpdyAltSvcIR& altsvc_ir,
                                 ZeroCopyOutputBuffer* output) {
  SpdyString value;
  size_t size = 0;
  SerializeAltSvcBuilderHelper(altsvc_ir, &value, &size);
  SpdyFrameBuilder builder(size, output);
  bool ok = builder.BeginNewFrame(SpdyFrameType::ALTSVC, kNoFlags,
                                  altsvc_ir.stream_id()) &&
            builder.WriteUInt16(altsvc_ir.origin().length()) &&
            builder.WriteBytes(altsvc_ir.origin().data(),
                               altsvc_ir.origin().length()) &&
            builder.WriteBytes(value.data(), value.length());
  DCHECK_LT(kGetAltSvcFrameMinimumSize, builder.length());
  return ok;
}

bool SpdyFramer::SerializePriority(const SpdyPriorityIR& priority,
                                   ZeroCopyOutputBuffer* output) const {
  SpdyFrameBuilder builder(kPriorityFrameSize, output);
  bool ok = builder.BeginNewFrame(SpdyFrameType::PRIORITY, kNoFlags,
                                  priority.stream_id());
  ok = ok &&
       builder.WriteUInt32(PackStreamDependencyValues(
           priority.exclusive(), priority.parent_stream_id())) &&
       // Per RFC 7540 section 6.3, serialized weight value is actual value - 1.
       builder.WriteUInt8(priority.weight() - 1);
  DCHECK_EQ(kPriorityFrameSize, builder.length());
  return ok;
}

bool SpdyFramer::SerializeUnknown(const SpdyUnknownIR& unknown,
                                  ZeroCopyOutputBuffer* output) const {
  const size_t total_size = kFrameHeaderSize + unknown.payload().size();
  SpdyFrameBuilder builder(total_size, output);
  bool ok = builder.BeginNewUncheckedFrame(
      unknown.type(), unknown.flags(), unknown.stream_id(), unknown.length());
  ok = ok &&
       builder.WriteBytes(unknown.payload().data(), unknown.payload().size());
  return ok;
}

namespace {

class FrameSerializationVisitorWithOutput : public SpdyFrameVisitor {
 public:
  explicit FrameSerializationVisitorWithOutput(SpdyFramer* framer,
                                               ZeroCopyOutputBuffer* output)
      : framer_(framer), output_(output), result_(false) {}
  ~FrameSerializationVisitorWithOutput() override = default;

  size_t Result() { return result_; }

  void VisitData(const SpdyDataIR& data) override {
    result_ = framer_->SerializeData(data, output_);
  }
  void VisitRstStream(const SpdyRstStreamIR& rst_stream) override {
    result_ = framer_->SerializeRstStream(rst_stream, output_);
  }
  void VisitSettings(const SpdySettingsIR& settings) override {
    result_ = framer_->SerializeSettings(settings, output_);
  }
  void VisitPing(const SpdyPingIR& ping) override {
    result_ = framer_->SerializePing(ping, output_);
  }
  void VisitGoAway(const SpdyGoAwayIR& goaway) override {
    result_ = framer_->SerializeGoAway(goaway, output_);
  }
  void VisitHeaders(const SpdyHeadersIR& headers) override {
    result_ = framer_->SerializeHeaders(headers, output_);
  }
  void VisitWindowUpdate(const SpdyWindowUpdateIR& window_update) override {
    result_ = framer_->SerializeWindowUpdate(window_update, output_);
  }
  void VisitPushPromise(const SpdyPushPromiseIR& push_promise) override {
    result_ = framer_->SerializePushPromise(push_promise, output_);
  }
  void VisitContinuation(const SpdyContinuationIR& continuation) override {
    result_ = framer_->SerializeContinuation(continuation, output_);
  }
  void VisitAltSvc(const SpdyAltSvcIR& altsvc) override {
    result_ = framer_->SerializeAltSvc(altsvc, output_);
  }
  void VisitPriority(const SpdyPriorityIR& priority) override {
    result_ = framer_->SerializePriority(priority, output_);
  }
  void VisitUnknown(const SpdyUnknownIR& unknown) override {
    result_ = framer_->SerializeUnknown(unknown, output_);
  }

 private:
  SpdyFramer* framer_;
  ZeroCopyOutputBuffer* output_;
  bool result_;
};

}  // namespace

size_t SpdyFramer::SerializeFrame(const SpdyFrameIR& frame,
                                  ZeroCopyOutputBuffer* output) {
  FrameSerializationVisitorWithOutput visitor(this, output);
  size_t free_bytes_before = output->BytesFree();
  frame.Visit(&visitor);
  return visitor.Result() ? free_bytes_before - output->BytesFree() : 0;
}

HpackEncoder* SpdyFramer::GetHpackEncoder() {
  if (hpack_encoder_.get() == nullptr) {
    hpack_encoder_ = SpdyMakeUnique<HpackEncoder>(ObtainHpackHuffmanTable());
    if (!compression_enabled()) {
      hpack_encoder_->DisableCompression();
    }
  }
  return hpack_encoder_.get();
}

void SpdyFramer::UpdateHeaderEncoderTableSize(uint32_t value) {
  GetHpackEncoder()->ApplyHeaderTableSizeSetting(value);
}

size_t SpdyFramer::header_encoder_table_size() const {
  if (hpack_encoder_ == nullptr) {
    return kDefaultHeaderTableSizeSetting;
  } else {
    return hpack_encoder_->CurrentHeaderTableSizeSetting();
  }
}

void SpdyFramer::SetEncoderHeaderTableDebugVisitor(
    std::unique_ptr<HpackHeaderTable::DebugVisitorInterface> visitor) {
  GetHpackEncoder()->SetHeaderTableDebugVisitor(std::move(visitor));
}

size_t SpdyFramer::EstimateMemoryUsage() const {
  return SpdyEstimateMemoryUsage(hpack_encoder_);
}

}  // namespace spdy
