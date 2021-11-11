// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

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
  DecodeTimestamp() {}
  DecodeTimestamp(const DecodeTimestamp& rhs) : ts_(rhs.ts_) { }
  DecodeTimestamp& operator=(const DecodeTimestamp& rhs) {
    if (&rhs != this)
      ts_ = rhs.ts_;
    return *this;
  }

  // Only operators that are actually used by the code have been defined.
  // Reviewers should pay close attention to the addition of new operators.
  bool operator<(const DecodeTimestamp& rhs) const { return ts_ < rhs.ts_; }
  bool operator>(const DecodeTimestamp& rhs) const  { return ts_ > rhs.ts_; }
  bool operator==(const DecodeTimestamp& rhs) const  { return ts_ == rhs.ts_; }
  bool operator!=(const DecodeTimestamp& rhs) const  { return ts_ != rhs.ts_; }
  bool operator>=(const DecodeTimestamp& rhs) const  { return ts_ >= rhs.ts_; }
  bool operator<=(const DecodeTimestamp& rhs) const  { return ts_ <= rhs.ts_; }

  base::TimeDelta operator-(const DecodeTimestamp& rhs) const {
    return ts_ - rhs.ts_;
  }

  DecodeTimestamp& operator+=(base::TimeDelta rhs) {
    ts_ += rhs;
    return *this;
  }

  DecodeTimestamp& operator-=(base::TimeDelta rhs) {
    ts_ -= rhs;
    return *this;
  }

  DecodeTimestamp operator+(base::TimeDelta rhs) const {
    return DecodeTimestamp(ts_ + rhs);
  }

  DecodeTimestamp operator-(base::TimeDelta rhs) const {
    return DecodeTimestamp(ts_ - rhs);
  }

  double operator/(base::TimeDelta rhs) const { return ts_ / rhs; }
  int64_t IntDiv(base::TimeDelta rhs) const { return ts_.IntDiv(rhs); }

  static DecodeTimestamp FromSecondsD(double seconds) {
    return DecodeTimestamp(base::Seconds(seconds));
  }

  static DecodeTimestamp FromMilliseconds(int64_t milliseconds) {
    return DecodeTimestamp(base::Milliseconds(milliseconds));
  }

  static DecodeTimestamp FromMicroseconds(int64_t microseconds) {
    return DecodeTimestamp(base::Microseconds(microseconds));
  }

  // This method is used to explicitly call out when presentation timestamps
  // are being converted to a decode timestamp.
  static DecodeTimestamp FromPresentationTime(base::TimeDelta timestamp) {
    return DecodeTimestamp(timestamp);
  }

  double InSecondsF() const { return ts_.InSecondsF(); }
  int64_t InMilliseconds() const { return ts_.InMilliseconds(); }
  int64_t InMicroseconds() const { return ts_.InMicroseconds(); }

  // TODO(acolwell): Remove once all the hacks are gone. This method is called
  // by hacks where a decode time is being used as a presentation time.
  base::TimeDelta ToPresentationTime() const { return ts_; }

 private:
  explicit DecodeTimestamp(base::TimeDelta timestamp) : ts_(timestamp) { }

  base::TimeDelta ts_;
};

MEDIA_EXPORT extern inline DecodeTimestamp kNoDecodeTimestamp() {
  return DecodeTimestamp::FromPresentationTime(kNoTimestamp);
}

class MEDIA_EXPORT StreamParserBuffer : public DecoderBuffer {
 public:
  // Value used to signal an invalid decoder config ID.
  enum { kInvalidConfigId = -1 };

  typedef DemuxerStream::Type Type;
  typedef StreamParser::TrackId TrackId;

  static scoped_refptr<StreamParserBuffer> CreateEOSBuffer();

  static scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                                    int data_size,
                                                    bool is_key_frame,
                                                    Type type,
                                                    TrackId track_id);
  static scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                                    int data_size,
                                                    const uint8_t* side_data,
                                                    int side_data_size,
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
  Type type() const { return type_; }
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
  void SetPrerollBuffer(scoped_refptr<StreamParserBuffer> preroll);
  scoped_refptr<StreamParserBuffer> preroll_buffer() { return preroll_buffer_; }

  void set_timestamp(base::TimeDelta timestamp) override;

  bool is_duration_estimated() const { return is_duration_estimated_; }

  void set_is_duration_estimated(bool is_estimated) {
    is_duration_estimated_ = is_estimated;
  }

 private:
  StreamParserBuffer(const uint8_t* data,
                     int data_size,
                     const uint8_t* side_data,
                     int side_data_size,
                     bool is_key_frame,
                     Type type,
                     TrackId track_id);
  ~StreamParserBuffer() override;

  DecodeTimestamp decode_timestamp_;
  int config_id_;
  Type type_;
  TrackId track_id_;
  scoped_refptr<StreamParserBuffer> preroll_buffer_;
  bool is_duration_estimated_;
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_H_
