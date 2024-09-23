// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/base/timestamp_constants.h"

namespace media {

// Simple wrapper around base::TimeDelta that represents a decode timestamp.
// Making DecodeTimestamp a different type makes it easier to determine whether
// code is operating on presentation or decode timestamps and makes conversions
// between the two types explicit and easy to spot.
class DecodeTimestamp {
 public:
  constexpr DecodeTimestamp() = default;

  // Only operators that are actually used by the code have been defined.
  // Reviewers should pay close attention to the addition of new operators.
  constexpr bool operator<(const DecodeTimestamp& rhs) const {
    return ts_ < rhs.ts_;
  }
  constexpr bool operator>(const DecodeTimestamp& rhs) const {
    return ts_ > rhs.ts_;
  }
  constexpr bool operator==(const DecodeTimestamp& rhs) const {
    return ts_ == rhs.ts_;
  }
  constexpr bool operator!=(const DecodeTimestamp& rhs) const {
    return ts_ != rhs.ts_;
  }
  constexpr bool operator>=(const DecodeTimestamp& rhs) const {
    return ts_ >= rhs.ts_;
  }
  constexpr bool operator<=(const DecodeTimestamp& rhs) const {
    return ts_ <= rhs.ts_;
  }

  constexpr base::TimeDelta operator-(const DecodeTimestamp& rhs) const {
    return ts_ - rhs.ts_;
  }

  constexpr DecodeTimestamp& operator+=(base::TimeDelta rhs) {
    ts_ += rhs;
    return *this;
  }

  constexpr DecodeTimestamp& operator-=(base::TimeDelta rhs) {
    ts_ -= rhs;
    return *this;
  }

  constexpr DecodeTimestamp operator+(base::TimeDelta rhs) const {
    return DecodeTimestamp(ts_ + rhs);
  }

  constexpr DecodeTimestamp operator-(base::TimeDelta rhs) const {
    return DecodeTimestamp(ts_ - rhs);
  }

  constexpr double operator/(base::TimeDelta rhs) const { return ts_ / rhs; }
  constexpr int64_t IntDiv(base::TimeDelta rhs) const {
    return ts_.IntDiv(rhs);
  }

  static constexpr DecodeTimestamp FromSecondsD(double seconds) {
    return DecodeTimestamp(base::Seconds(seconds));
  }

  static constexpr DecodeTimestamp FromMilliseconds(int64_t milliseconds) {
    return DecodeTimestamp(base::Milliseconds(milliseconds));
  }

  static constexpr DecodeTimestamp FromMicroseconds(int64_t microseconds) {
    return DecodeTimestamp(base::Microseconds(microseconds));
  }

  // This method is used to explicitly call out when presentation timestamps
  // are being converted to a decode timestamp.
  static constexpr DecodeTimestamp FromPresentationTime(
      base::TimeDelta timestamp) {
    return DecodeTimestamp(timestamp);
  }

  constexpr double InSecondsF() const { return ts_.InSecondsF(); }
  int64_t InMilliseconds() const { return ts_.InMilliseconds(); }
  constexpr int64_t InMicroseconds() const { return ts_.InMicroseconds(); }

  constexpr bool is_inf() const { return ts_.is_inf(); }

  // TODO(acolwell): Remove once all the hacks are gone. This method is called
  // by hacks where a decode time is being used as a presentation time.
  constexpr base::TimeDelta ToPresentationTime() const { return ts_; }

 private:
  constexpr explicit DecodeTimestamp(base::TimeDelta timestamp)
      : ts_(timestamp) {}

  base::TimeDelta ts_;
};

// Assert assumptions necessary for DecodeTimestamp analogues of
// base::TimeDelta::is_inf(), media::kNoTimestamp and media::kInfiniteDuration.
static_assert(kNoTimestamp.is_min() && kNoTimestamp.is_inf());
static_assert(kInfiniteDuration.is_max() && kInfiniteDuration.is_inf());

// Indicates an invalid or missing decode timestamp.
constexpr DecodeTimestamp kNoDecodeTimestamp =
    DecodeTimestamp::FromPresentationTime(kNoTimestamp);

// Similar to media::kInfiniteDuration, indicates a decode timestamp of positive
// infinity.
constexpr DecodeTimestamp kMaxDecodeTimestamp =
    DecodeTimestamp::FromPresentationTime(kInfiniteDuration);

class MEDIA_EXPORT StreamParserBuffer : public DecoderBuffer {
 public:
  // Value used to signal an invalid decoder config ID.
  enum { kInvalidConfigId = -1 };

  typedef DemuxerStream::Type Type;
  typedef StreamParser::TrackId TrackId;

  static scoped_refptr<StreamParserBuffer> CreateEOSBuffer(
      std::optional<ConfigVariant> next_config = std::nullopt);

  static scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                                    int data_size,
                                                    bool is_key_frame,
                                                    Type type,
                                                    TrackId track_id);
  static scoped_refptr<StreamParserBuffer> FromExternalMemory(
      std::unique_ptr<ExternalMemory> external_memory,
      bool is_key_frame,
      Type type,
      TrackId track_id);
  static scoped_refptr<StreamParserBuffer> FromArray(
      base::HeapArray<uint8_t> heap_array,
      bool is_key_frame,
      Type type,
      TrackId track_id);

  StreamParserBuffer(const StreamParserBuffer&) = delete;
  StreamParserBuffer& operator=(const StreamParserBuffer&) = delete;

  // Decode timestamp. If not explicitly set, or set to kNoTimestamp, the
  // value will be taken from the normal timestamp.
  DecodeTimestamp GetDecodeTimestamp() const;
  void SetDecodeTimestamp(DecodeTimestamp timestamp);

  // Gets/sets the ID of the decoder config associated with this buffer.
  int GetConfigId() const;
  void SetConfigId(int config_id);

  // Gets the parser's media type associated with this buffer. Value is
  // meaningless for EOS buffers.
  Type type() const { return static_cast<Type>(type_); }
  const char* GetTypeName() const;

  // Gets the parser's track ID associated with this buffer. Value is
  // meaningless for EOS buffers.
  TrackId track_id() const { return track_id_; }

  // Specifies a buffer which must be decoded prior to this one to ensure this
  // buffer can be accurately decoded.  The given buffer must be of the same
  // type, must not have any discard padding, and must not be an end of stream
  // buffer.  |preroll| is not copied.
  //
  // It's expected that this preroll buffer will be discarded entirely post
  // decoding.  As such it's discard_padding() will be set to kInfiniteDuration.
  //
  // All future timestamp, decode timestamp, config id, or track id changes to
  // this buffer will be applied to the preroll buffer as well.
  //
  // TODO(b/331652782): integrate the setter function into the constructor to
  // make |preroll_buffer_| immutable.
  void SetPrerollBuffer(scoped_refptr<StreamParserBuffer> preroll);
  scoped_refptr<StreamParserBuffer> preroll_buffer() { return preroll_buffer_; }

  void set_timestamp(base::TimeDelta timestamp) override;

  bool is_duration_estimated() const { return is_duration_estimated_; }

  void set_is_duration_estimated(bool is_estimated) {
    is_duration_estimated_ = is_estimated;
  }

  size_t GetMemoryUsage() const override;

 private:
  StreamParserBuffer(base::HeapArray<uint8_t> heap_array,
                     bool is_key_frame,
                     Type type,
                     TrackId track_id);

  StreamParserBuffer(std::unique_ptr<ExternalMemory> external_memory,
                     bool is_key_frame,
                     Type type,
                     TrackId track_id);
  StreamParserBuffer(const uint8_t* data,
                     int data_size,
                     bool is_key_frame,
                     Type type,
                     TrackId track_id);
  StreamParserBuffer(DecoderBufferType decoder_buffer_type,
                     std::optional<ConfigVariant> next_config);
  ~StreamParserBuffer() override;

  // ***************************************************************************
  // WARNING: This is a highly allocated object. Care should be taken when
  // adding any fields to make sure they are absolutely necessary. If a field
  // must be added and can be optional, ensure it is heap allocated through the
  // usage of something like std::unique_ptr.
  // ***************************************************************************

  // Note: This field is stored as a uint8_t instead of Type and uses
  // static_cast<Type> in type() to avoid signed vs unsigned issues when Type
  // is directly used as a bit-field.
  const uint8_t type_ : 2;

  bool is_duration_estimated_ : 1 = false;
  DecodeTimestamp decode_timestamp_ = kNoDecodeTimestamp;
  int config_id_ = kInvalidConfigId;
  const TrackId track_id_;
  scoped_refptr<StreamParserBuffer> preroll_buffer_;
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_H_
