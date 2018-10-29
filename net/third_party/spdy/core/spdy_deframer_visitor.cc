// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/spdy_deframer_visitor.h"

#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <limits>

#include "base/logging.h"
#include "net/third_party/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/spdy/core/mock_spdy_framer_visitor.h"
#include "net/third_party/spdy/core/spdy_frame_reader.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"
#include "net/third_party/spdy/platform/api/spdy_flags.h"
#include "net/third_party/spdy/platform/api/spdy_ptr_util.h"
#include "net/third_party/spdy/platform/api/spdy_string_piece.h"

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

namespace spdy {
namespace test {

// Specify whether to process headers as request or response in visitor-related
// params.
enum class HeaderDirection { REQUEST, RESPONSE };

// Types of HTTP/2 frames, per RFC 7540.
// TODO(jamessynge): Switch to using //net/third_party/http2/http2_constants.h
// when ready.
enum Http2FrameType {
  DATA = 0,
  HEADERS = 1,
  PRIORITY = 2,
  RST_STREAM = 3,
  SETTINGS = 4,
  PUSH_PROMISE = 5,
  PING = 6,
  GOAWAY = 7,
  WINDOW_UPDATE = 8,
  CONTINUATION = 9,
  ALTSVC = 10,

  // Not a frame type.
  UNSET = -1,
  UNKNOWN = -2,
};

// TODO(jamessynge): Switch to using //net/third_party/http2/http2_constants.h
// when ready.
const char* Http2FrameTypeToString(Http2FrameType v) {
  switch (v) {
    case DATA:
      return "DATA";
    case HEADERS:
      return "HEADERS";
    case PRIORITY:
      return "PRIORITY";
    case RST_STREAM:
      return "RST_STREAM";
    case SETTINGS:
      return "SETTINGS";
    case PUSH_PROMISE:
      return "PUSH_PROMISE";
    case PING:
      return "PING";
    case GOAWAY:
      return "GOAWAY";
    case WINDOW_UPDATE:
      return "WINDOW_UPDATE";
    case CONTINUATION:
      return "CONTINUATION";
    case ALTSVC:
      return "ALTSVC";
    case UNSET:
      return "UNSET";
    case UNKNOWN:
      return "UNKNOWN";
    default:
      return "Invalid Http2FrameType";
  }
}

// TODO(jamessynge): Switch to using //net/third_party/http2/http2_constants.h
// when ready.
inline std::ostream& operator<<(std::ostream& out, Http2FrameType v) {
  return out << Http2FrameTypeToString(v);
}

// Flag bits in the flag field of the common header of HTTP/2 frames
// (see https://httpwg.github.io/specs/rfc7540.html#FrameHeader for details on
// the fixed 9-octet header structure shared by all frames).
// Flag bits are only valid for specified frame types.
// TODO(jamessynge): Switch to using //net/third_party/http2/http2_constants.h
// when ready.
enum Http2HeaderFlag {
  NO_FLAGS = 0,

  END_STREAM_FLAG = 0x1,
  ACK_FLAG = 0x1,
  END_HEADERS_FLAG = 0x4,
  PADDED_FLAG = 0x8,
  PRIORITY_FLAG = 0x20,
};

// Returns name of frame type.
// TODO(jamessynge): Switch to using //net/third_party/http2/http2_constants.h
// when ready.
const char* Http2FrameTypeToString(Http2FrameType v);

void SpdyDeframerVisitorInterface::OnPingAck(
    std::unique_ptr<SpdyPingIR> frame) {
  OnPing(std::move(frame));
}

void SpdyDeframerVisitorInterface::OnSettingsAck(
    std::unique_ptr<SpdySettingsIR> frame) {
  OnSettings(std::move(frame), nullptr);
}

class SpdyTestDeframerImpl : public SpdyTestDeframer,
                             public SpdyHeadersHandlerInterface {
 public:
  explicit SpdyTestDeframerImpl(
      std::unique_ptr<SpdyDeframerVisitorInterface> listener)
      : listener_(std::move(listener)) {
    CHECK(listener_ != nullptr);
  }
  SpdyTestDeframerImpl(const SpdyTestDeframerImpl&) = delete;
  SpdyTestDeframerImpl& operator=(const SpdyTestDeframerImpl&) = delete;
  ~SpdyTestDeframerImpl() override = default;

  bool AtFrameEnd() override;

  // Callbacks defined in SpdyFramerVisitorInterface.  These are in the
  // alphabetical order for ease of navigation, and are not in same order
  // as in SpdyFramerVisitorInterface.
  void OnAltSvc(SpdyStreamId stream_id,
                SpdyStringPiece origin,
                const SpdyAltSvcWireFormat::AlternativeServiceVector&
                    altsvc_vector) override;
  void OnContinuation(SpdyStreamId stream_id, bool end) override;
  SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      SpdyStreamId stream_id) override;
  void OnHeaderFrameEnd(SpdyStreamId stream_id) override;
  void OnDataFrameHeader(SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override;
  void OnError(http2::Http2DecoderAdapter::SpdyFramerError error) override;
  void OnGoAway(SpdyStreamId last_accepted_stream_id,
                SpdyErrorCode error_code) override;
  bool OnGoAwayFrameData(const char* goaway_data, size_t len) override;
  void OnHeaders(SpdyStreamId stream_id,
                 bool has_priority,
                 int weight,
                 SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 bool end) override;
  void OnPing(SpdyPingId unique_id, bool is_ack) override;
  void OnPriority(SpdyStreamId stream_id,
                  SpdyStreamId parent_stream_id,
                  int weight,
                  bool exclusive) override;
  void OnPushPromise(SpdyStreamId stream_id,
                     SpdyStreamId promised_stream_id,
                     bool end) override;
  void OnRstStream(SpdyStreamId stream_id, SpdyErrorCode error_code) override;
  void OnSetting(SpdySettingsId id, uint32_t value) override;
  void OnSettings() override;
  void OnSettingsAck() override;
  void OnSettingsEnd() override;
  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override;
  void OnStreamEnd(SpdyStreamId stream_id) override;
  void OnStreamPadLength(SpdyStreamId stream_id, size_t value) override;
  void OnStreamPadding(SpdyStreamId stream_id, size_t len) override;
  bool OnUnknownFrame(SpdyStreamId stream_id, uint8_t frame_type) override;
  void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) override;

  // Callbacks defined in SpdyHeadersHandlerInterface.

  void OnHeaderBlockStart() override;
  void OnHeader(SpdyStringPiece key, SpdyStringPiece value) override;
  void OnHeaderBlockEnd(size_t header_bytes_parsed,
                        size_t compressed_header_bytes_parsed) override;

 protected:
  void AtDataEnd();
  void AtGoAwayEnd();
  void AtHeadersEnd();
  void AtPushPromiseEnd();

  // Per-physical frame state.
  // Frame type of the frame currently being processed.
  Http2FrameType frame_type_ = UNSET;
  // Stream id of the frame currently being processed.
  SpdyStreamId stream_id_;
  // Did the most recent frame header include the END_HEADERS flag?
  bool end_ = false;
  // Did the most recent frame header include the ack flag?
  bool ack_ = false;

  // Per-HPACK block state. Only valid while processing a HEADERS or
  // PUSH_PROMISE frame, and its CONTINUATION frames.
  // Did the most recent HEADERS or PUSH_PROMISE include the END_STREAM flag?
  // Note that this does not necessarily indicate that the current frame is
  // the last frame for the stream (may be followed by CONTINUATION frames,
  // may only half close).
  bool fin_ = false;
  bool got_hpack_end_ = false;

  std::unique_ptr<SpdyString> data_;

  // Total length of the data frame.
  size_t data_len_ = 0;

  // Amount of skipped padding (i.e. total length of padding, including Pad
  // Length field).
  size_t padding_len_ = 0;

  std::unique_ptr<SpdyString> goaway_description_;
  std::unique_ptr<StringPairVector> headers_;
  std::unique_ptr<SettingVector> settings_;
  std::unique_ptr<TestHeadersHandler> headers_handler_;

  std::unique_ptr<SpdyGoAwayIR> goaway_ir_;
  std::unique_ptr<SpdyHeadersIR> headers_ir_;
  std::unique_ptr<SpdyPushPromiseIR> push_promise_ir_;
  std::unique_ptr<SpdySettingsIR> settings_ir_;

 private:
  std::unique_ptr<SpdyDeframerVisitorInterface> listener_;
};

// static
std::unique_ptr<SpdyTestDeframer> SpdyTestDeframer::CreateConverter(
    std::unique_ptr<SpdyDeframerVisitorInterface> listener) {
  return SpdyMakeUnique<SpdyTestDeframerImpl>(std::move(listener));
}

void SpdyTestDeframerImpl::AtDataEnd() {
  DVLOG(1) << "AtDataEnd";
  CHECK_EQ(data_len_, padding_len_ + data_->size());
  auto ptr = SpdyMakeUnique<SpdyDataIR>(stream_id_, std::move(*data_));
  CHECK_EQ(0u, data_->size());
  data_.reset();

  CHECK_LE(0u, padding_len_);
  CHECK_LE(padding_len_, 256u);
  if (padding_len_ > 0) {
    ptr->set_padding_len(padding_len_);
  }
  padding_len_ = 0;

  ptr->set_fin(fin_);
  listener_->OnData(std::move(ptr));
  frame_type_ = UNSET;
  fin_ = false;
  data_len_ = 0;
}

void SpdyTestDeframerImpl::AtGoAwayEnd() {
  DVLOG(1) << "AtDataEnd";
  CHECK_EQ(frame_type_, GOAWAY);
  CHECK(goaway_description_);
  if (goaway_description_->empty()) {
    listener_->OnGoAway(std::move(goaway_ir_));
  } else {
    listener_->OnGoAway(SpdyMakeUnique<SpdyGoAwayIR>(
        goaway_ir_->last_good_stream_id(), goaway_ir_->error_code(),
        std::move(*goaway_description_)));
    CHECK_EQ(0u, goaway_description_->size());
  }
  goaway_description_.reset();
  goaway_ir_.reset();
  frame_type_ = UNSET;
}

void SpdyTestDeframerImpl::AtHeadersEnd() {
  DVLOG(1) << "AtDataEnd";
  CHECK(frame_type_ == HEADERS || frame_type_ == CONTINUATION)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(end_) << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(got_hpack_end_);

  CHECK(headers_ir_ != nullptr);
  CHECK(headers_ != nullptr);
  CHECK(headers_handler_ != nullptr);

  CHECK_LE(0u, padding_len_);
  CHECK_LE(padding_len_, 256u);
  if (padding_len_ > 0) {
    headers_ir_->set_padding_len(padding_len_);
  }
  padding_len_ = 0;

  headers_ir_->set_header_block(headers_handler_->decoded_block().Clone());
  headers_handler_.reset();
  listener_->OnHeaders(std::move(headers_ir_), std::move(headers_));

  frame_type_ = UNSET;
  fin_ = false;
  end_ = false;
  got_hpack_end_ = false;
}

void SpdyTestDeframerImpl::AtPushPromiseEnd() {
  DVLOG(1) << "AtDataEnd";
  CHECK(frame_type_ == PUSH_PROMISE || frame_type_ == CONTINUATION)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(end_) << "   frame_type_=" << Http2FrameTypeToString(frame_type_);

  CHECK(push_promise_ir_ != nullptr);
  CHECK(headers_ != nullptr);
  CHECK(headers_handler_ != nullptr);

  CHECK_EQ(headers_ir_.get(), nullptr);

  CHECK_LE(0u, padding_len_);
  CHECK_LE(padding_len_, 256u);
  if (padding_len_ > 0) {
    push_promise_ir_->set_padding_len(padding_len_);
  }
  padding_len_ = 0;

  push_promise_ir_->set_header_block(headers_handler_->decoded_block().Clone());
  headers_handler_.reset();
  listener_->OnPushPromise(std::move(push_promise_ir_), std::move(headers_));

  frame_type_ = UNSET;
  end_ = false;
}

bool SpdyTestDeframerImpl::AtFrameEnd() {
  bool incomplete_logical_header = false;
  // The caller says that the SpdyFrame has reached the end of the frame,
  // so if we have any accumulated data, flush it.
  switch (frame_type_) {
    case DATA:
      AtDataEnd();
      break;

    case GOAWAY:
      AtGoAwayEnd();
      break;

    case HEADERS:
      if (end_) {
        AtHeadersEnd();
      } else {
        incomplete_logical_header = true;
      }
      break;

    case PUSH_PROMISE:
      if (end_) {
        AtPushPromiseEnd();
      } else {
        incomplete_logical_header = true;
      }
      break;

    case CONTINUATION:
      if (end_) {
        if (headers_ir_) {
          AtHeadersEnd();
        } else if (push_promise_ir_) {
          AtPushPromiseEnd();
        } else {
          LOG(FATAL) << "Where is the SpdyFrameIR for the headers!";
        }
      } else {
        incomplete_logical_header = true;
      }
      break;

    case UNSET:
      // Except for the frame types above, the others don't leave any record
      // in the state of this object. Make sure nothing got left by accident.
      CHECK_EQ(data_.get(), nullptr);
      CHECK_EQ(goaway_description_.get(), nullptr);
      CHECK_EQ(goaway_ir_.get(), nullptr);
      CHECK_EQ(headers_.get(), nullptr);
      CHECK_EQ(headers_handler_.get(), nullptr);
      CHECK_EQ(headers_ir_.get(), nullptr);
      CHECK_EQ(push_promise_ir_.get(), nullptr);
      CHECK_EQ(settings_.get(), nullptr);
      CHECK_EQ(settings_ir_.get(), nullptr);
      break;

    default:
      SPDY_BUG << "Expected UNSET, instead frame_type_==" << frame_type_;
      return false;
  }
  frame_type_ = UNSET;
  stream_id_ = 0;
  end_ = false;
  ack_ = false;
  if (!incomplete_logical_header) {
    fin_ = false;
  }
  return true;
}

// Overridden methods from SpdyFramerVisitorInterface in alpha order...

void SpdyTestDeframerImpl::OnAltSvc(
    SpdyStreamId stream_id,
    SpdyStringPiece origin,
    const SpdyAltSvcWireFormat::AlternativeServiceVector& altsvc_vector) {
  DVLOG(1) << "OnAltSvc stream_id: " << stream_id;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_GT(stream_id, 0u);
  auto ptr = SpdyMakeUnique<SpdyAltSvcIR>(stream_id);
  ptr->set_origin(SpdyString(origin));
  for (auto& altsvc : altsvc_vector) {
    ptr->add_altsvc(altsvc);
  }
  listener_->OnAltSvc(std::move(ptr));
}

// A CONTINUATION frame contains a Header Block Fragment, and immediately
// follows another frame that contains a Header Block Fragment (HEADERS,
// PUSH_PROMISE or CONTINUATION). The last such frame has the END flag set.
// SpdyFramer ensures that the behavior is correct before calling the visitor.
void SpdyTestDeframerImpl::OnContinuation(SpdyStreamId stream_id, bool end) {
  DVLOG(1) << "OnContinuation stream_id: " << stream_id;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_GT(stream_id, 0u);
  CHECK_NE(nullptr, headers_.get());
  frame_type_ = CONTINUATION;

  stream_id_ = stream_id;
  end_ = end;
}

// Note that length includes the padding length (0 to 256, when the optional
// padding length field is counted). Padding comes after the payload, both
// for DATA frames and for control frames.
void SpdyTestDeframerImpl::OnDataFrameHeader(SpdyStreamId stream_id,
                                             size_t length,
                                             bool fin) {
  DVLOG(1) << "OnDataFrameHeader stream_id: " << stream_id;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_GT(stream_id, 0u);
  CHECK_EQ(data_.get(), nullptr);
  frame_type_ = DATA;

  stream_id_ = stream_id;
  fin_ = fin;
  data_len_ = length;
  data_ = SpdyMakeUnique<SpdyString>();
}

// The SpdyFramer will not process any more data at this point.
void SpdyTestDeframerImpl::OnError(
    http2::Http2DecoderAdapter::SpdyFramerError error) {
  DVLOG(1) << "SpdyFramer detected an error in the stream: "
           << http2::Http2DecoderAdapter::SpdyFramerErrorToString(error)
           << "     frame_type_: " << Http2FrameTypeToString(frame_type_);
  listener_->OnError(error, this);
}

// Received a GOAWAY frame from the peer. The last stream id it accepted from us
// is |last_accepted_stream_id|. |status| is a protocol defined error code.
// The frame may also contain data. After this OnGoAwayFrameData will be called
// for any non-zero amount of data, and after that it will be called with len==0
// to indicate the end of the GOAWAY frame.
void SpdyTestDeframerImpl::OnGoAway(SpdyStreamId last_good_stream_id,
                                    SpdyErrorCode error_code) {
  DVLOG(1) << "OnGoAway last_good_stream_id: " << last_good_stream_id
           << "     error code: " << error_code;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  frame_type_ = GOAWAY;
  goaway_ir_ =
      SpdyMakeUnique<SpdyGoAwayIR>(last_good_stream_id, error_code, "");
  goaway_description_ = SpdyMakeUnique<SpdyString>();
}

// If len==0 then we've reached the end of the GOAWAY frame.
bool SpdyTestDeframerImpl::OnGoAwayFrameData(const char* goaway_data,
                                             size_t len) {
  DVLOG(1) << "OnGoAwayFrameData";
  CHECK_EQ(frame_type_, GOAWAY)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(goaway_description_ != nullptr);
  goaway_description_->append(goaway_data, len);
  return true;
}

SpdyHeadersHandlerInterface* SpdyTestDeframerImpl::OnHeaderFrameStart(
    SpdyStreamId stream_id) {
  return this;
}

void SpdyTestDeframerImpl::OnHeaderFrameEnd(SpdyStreamId stream_id) {
  DVLOG(1) << "OnHeaderFrameEnd stream_id: " << stream_id;
}

// Received the fixed portion of a HEADERS frame. Called before the variable
// length (including zero length) Header Block Fragment is processed. If fin
// is true then there will be no DATA or trailing HEADERS after this HEADERS
// frame.
// If end is true, then there will be no CONTINUATION frame(s) following this
// frame; else if true then there will be CONTINATION frames(s) immediately
// following this frame, terminated by a CONTINUATION frame with end==true.
void SpdyTestDeframerImpl::OnHeaders(SpdyStreamId stream_id,
                                     bool has_priority,
                                     int weight,
                                     SpdyStreamId parent_stream_id,
                                     bool exclusive,
                                     bool fin,
                                     bool end) {
  DVLOG(1) << "OnHeaders stream_id: " << stream_id;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_GT(stream_id, 0u);
  frame_type_ = HEADERS;

  stream_id_ = stream_id;
  fin_ = fin;
  end_ = end;

  headers_ = SpdyMakeUnique<StringPairVector>();
  headers_handler_ = SpdyMakeUnique<TestHeadersHandler>();
  headers_ir_ = SpdyMakeUnique<SpdyHeadersIR>(stream_id);
  headers_ir_->set_fin(fin);
  if (has_priority) {
    headers_ir_->set_has_priority(true);
    headers_ir_->set_weight(weight);
    headers_ir_->set_parent_stream_id(parent_stream_id);
    headers_ir_->set_exclusive(exclusive);
  }
}

// The HTTP/2 protocol refers to the payload, |unique_id| here, as 8 octets of
// opaque data that is to be echoed back to the sender, with the ACK bit added.
// It isn't defined as a counter,
// or frame id, as the SpdyPingId naming might imply.
// Responding to a PING is supposed to be at the highest priority.
void SpdyTestDeframerImpl::OnPing(uint64_t unique_id, bool is_ack) {
  DVLOG(1) << "OnPing unique_id: " << unique_id
           << "      is_ack: " << (is_ack ? "true" : "false");
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  auto ptr = SpdyMakeUnique<SpdyPingIR>(unique_id);
  if (is_ack) {
    ptr->set_is_ack(is_ack);
    listener_->OnPingAck(std::move(ptr));
  } else {
    listener_->OnPing(std::move(ptr));
  }
}

void SpdyTestDeframerImpl::OnPriority(SpdyStreamId stream_id,
                                      SpdyStreamId parent_stream_id,
                                      int weight,
                                      bool exclusive) {
  DVLOG(1) << "OnPriority stream_id: " << stream_id;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_GT(stream_id, 0u);

  listener_->OnPriority(SpdyMakeUnique<SpdyPriorityIR>(
      stream_id, parent_stream_id, weight, exclusive));
}

void SpdyTestDeframerImpl::OnPushPromise(SpdyStreamId stream_id,
                                         SpdyStreamId promised_stream_id,
                                         bool end) {
  DVLOG(1) << "OnPushPromise stream_id: " << stream_id;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_GT(stream_id, 0u);

  frame_type_ = PUSH_PROMISE;
  stream_id_ = stream_id;
  end_ = end;

  headers_ = SpdyMakeUnique<StringPairVector>();
  headers_handler_ = SpdyMakeUnique<TestHeadersHandler>();
  push_promise_ir_ =
      SpdyMakeUnique<SpdyPushPromiseIR>(stream_id, promised_stream_id);
}

// Closes the specified stream. After this the sender may still send PRIORITY
// frames for this stream, which we can ignore.
void SpdyTestDeframerImpl::OnRstStream(SpdyStreamId stream_id,
                                       SpdyErrorCode error_code) {
  DVLOG(1) << "OnRstStream stream_id: " << stream_id
           << "     error code: " << error_code;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_GT(stream_id, 0u);

  listener_->OnRstStream(
      SpdyMakeUnique<SpdyRstStreamIR>(stream_id, error_code));
}

// Called for an individual setting. There is no negotiation; the sender is
// stating the value that the sender is using.
void SpdyTestDeframerImpl::OnSetting(SpdySettingsId id, uint32_t value) {
  DVLOG(1) << "OnSetting id: " << id << std::hex << "    value: " << value;
  CHECK_EQ(frame_type_, SETTINGS)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(settings_ != nullptr);
  SpdyKnownSettingsId known_id;
  if (ParseSettingsId(id, &known_id)) {
    settings_->push_back(std::make_pair(known_id, value));
    settings_ir_->AddSetting(known_id, value);
  }
}

// Called at the start of a SETTINGS frame with setting entries, but not the
// (required) ACK of a SETTINGS frame. There is no stream_id because
// the settings apply to the entire connection, not to an individual stream.
void SpdyTestDeframerImpl::OnSettings() {
  DVLOG(1) << "OnSettings";
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_EQ(nullptr, settings_ir_.get());
  CHECK_EQ(nullptr, settings_.get());
  frame_type_ = SETTINGS;
  ack_ = false;

  settings_ = SpdyMakeUnique<SettingVector>();
  settings_ir_ = SpdyMakeUnique<SpdySettingsIR>();
}

void SpdyTestDeframerImpl::OnSettingsAck() {
  DVLOG(1) << "OnSettingsAck";
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  auto ptr = SpdyMakeUnique<SpdySettingsIR>();
  ptr->set_is_ack(true);
  listener_->OnSettingsAck(std::move(ptr));
}

void SpdyTestDeframerImpl::OnSettingsEnd() {
  DVLOG(1) << "OnSettingsEnd";
  CHECK_EQ(frame_type_, SETTINGS)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(!ack_);
  CHECK_NE(nullptr, settings_ir_.get());
  CHECK_NE(nullptr, settings_.get());
  listener_->OnSettings(std::move(settings_ir_), std::move(settings_));
  frame_type_ = UNSET;
}

// Called for a zero length DATA frame with the END_STREAM flag set, or at the
// end a complete HPACK block (and its padding) that started with a HEADERS
// frame with the END_STREAM flag set. Doesn't apply to PUSH_PROMISE frames
// because they don't have END_STREAM flags.
void SpdyTestDeframerImpl::OnStreamEnd(SpdyStreamId stream_id) {
  DVLOG(1) << "OnStreamEnd stream_id: " << stream_id;
  CHECK_EQ(stream_id_, stream_id);
  CHECK(frame_type_ == DATA || frame_type_ == HEADERS ||
        frame_type_ == CONTINUATION)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(fin_);
}

// The data arg points into the non-padding payload of a DATA frame.
// This must be a DATA frame (i.e. this method will not be
// called for HEADERS or CONTINUATION frames).
// This method may be called multiple times for a single DATA frame, depending
// upon buffer boundaries.
void SpdyTestDeframerImpl::OnStreamFrameData(SpdyStreamId stream_id,
                                             const char* data,
                                             size_t len) {
  DVLOG(1) << "OnStreamFrameData stream_id: " << stream_id
           << "    len: " << len;
  CHECK_EQ(stream_id_, stream_id);
  CHECK_EQ(frame_type_, DATA);
  data_->append(data, len);
}

// Called when receiving the padding length field at the start of the DATA frame
// payload. value will be in the range 0 to 255.
void SpdyTestDeframerImpl::OnStreamPadLength(SpdyStreamId stream_id,
                                             size_t value) {
  DVLOG(1) << "OnStreamPadding stream_id: " << stream_id
           << "    value: " << value;
  CHECK(frame_type_ == DATA || frame_type_ == HEADERS ||
        frame_type_ == PUSH_PROMISE)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_EQ(stream_id_, stream_id);
  CHECK_GE(255u, value);
  // Count the padding length byte against total padding.
  padding_len_ += 1;
  CHECK_EQ(1u, padding_len_);
}

// Called when padding is skipped over at the end of the DATA frame. len will
// be in the range 1 to 255.
void SpdyTestDeframerImpl::OnStreamPadding(SpdyStreamId stream_id, size_t len) {
  DVLOG(1) << "OnStreamPadding stream_id: " << stream_id << "    len: " << len;
  CHECK(frame_type_ == DATA || frame_type_ == HEADERS ||
        frame_type_ == PUSH_PROMISE)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_EQ(stream_id_, stream_id);
  CHECK_LE(1u, len);
  CHECK_GE(255u, len);
  padding_len_ += len;
  CHECK_LE(padding_len_, 256u) << "len=" << len;
}

// WINDOW_UPDATE is supposed to be hop-by-hop, according to the spec.
// stream_id is 0 if the update applies to the connection, else stream_id
// will be the id of a stream previously seen, which maybe half or fully
// closed.
void SpdyTestDeframerImpl::OnWindowUpdate(SpdyStreamId stream_id,
                                          int delta_window_size) {
  DVLOG(1) << "OnWindowUpdate stream_id: " << stream_id
           << "    delta_window_size: " << delta_window_size;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK_NE(0, delta_window_size);

  listener_->OnWindowUpdate(
      SpdyMakeUnique<SpdyWindowUpdateIR>(stream_id, delta_window_size));
}

// Return true to indicate that the stream_id is valid; if not valid then
// SpdyFramer considers the connection corrupted. Requires keeping track
// of the set of currently open streams. For now we'll assume that unknown
// frame types are unsupported.
bool SpdyTestDeframerImpl::OnUnknownFrame(SpdyStreamId stream_id,
                                          uint8_t frame_type) {
  DVLOG(1) << "OnAltSvc stream_id: " << stream_id;
  CHECK_EQ(frame_type_, UNSET)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  frame_type_ = UNKNOWN;

  stream_id_ = stream_id;
  return false;
}

// Callbacks defined in SpdyHeadersHandlerInterface.

void SpdyTestDeframerImpl::OnHeaderBlockStart() {
  CHECK(frame_type_ == HEADERS || frame_type_ == PUSH_PROMISE)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(headers_ != nullptr);
  CHECK_EQ(0u, headers_->size());
  got_hpack_end_ = false;
}

void SpdyTestDeframerImpl::OnHeader(SpdyStringPiece key,
                                    SpdyStringPiece value) {
  CHECK(frame_type_ == HEADERS || frame_type_ == CONTINUATION ||
        frame_type_ == PUSH_PROMISE)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(!got_hpack_end_);
  CHECK(headers_);
  headers_->emplace_back(SpdyString(key), SpdyString(value));
  CHECK(headers_handler_);
  headers_handler_->OnHeader(key, value);
}

void SpdyTestDeframerImpl::OnHeaderBlockEnd(
    size_t /* header_bytes_parsed */,
    size_t /* compressed_header_bytes_parsed */) {
  CHECK(headers_);
  CHECK(frame_type_ == HEADERS || frame_type_ == CONTINUATION ||
        frame_type_ == PUSH_PROMISE)
      << "   frame_type_=" << Http2FrameTypeToString(frame_type_);
  CHECK(end_);
  CHECK(!got_hpack_end_);
  got_hpack_end_ = true;
}

class LoggingSpdyDeframerDelegate : public SpdyDeframerVisitorInterface {
 public:
  explicit LoggingSpdyDeframerDelegate(
      std::unique_ptr<SpdyDeframerVisitorInterface> wrapped)
      : wrapped_(std::move(wrapped)) {
    if (!wrapped_) {
      wrapped_ = SpdyMakeUnique<SpdyDeframerVisitorInterface>();
    }
  }
  ~LoggingSpdyDeframerDelegate() override = default;

  void OnAltSvc(std::unique_ptr<SpdyAltSvcIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnAltSvc";
    wrapped_->OnAltSvc(std::move(frame));
  }
  void OnData(std::unique_ptr<SpdyDataIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnData";
    wrapped_->OnData(std::move(frame));
  }
  void OnGoAway(std::unique_ptr<SpdyGoAwayIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnGoAway";
    wrapped_->OnGoAway(std::move(frame));
  }

  // SpdyHeadersIR and SpdyPushPromiseIR each has a SpdyHeaderBlock which
  // significantly modifies the headers, so the actual header entries (name
  // and value strings) are provided in a vector.
  void OnHeaders(std::unique_ptr<SpdyHeadersIR> frame,
                 std::unique_ptr<StringPairVector> headers) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnHeaders";
    wrapped_->OnHeaders(std::move(frame), std::move(headers));
  }

  void OnPing(std::unique_ptr<SpdyPingIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnPing";
    wrapped_->OnPing(std::move(frame));
  }
  void OnPingAck(std::unique_ptr<SpdyPingIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnPingAck";
    wrapped_->OnPingAck(std::move(frame));
  }

  void OnPriority(std::unique_ptr<SpdyPriorityIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnPriority";
    wrapped_->OnPriority(std::move(frame));
  }

  // SpdyHeadersIR and SpdyPushPromiseIR each has a SpdyHeaderBlock which
  // significantly modifies the headers, so the actual header entries (name
  // and value strings) are provided in a vector.
  void OnPushPromise(std::unique_ptr<SpdyPushPromiseIR> frame,
                     std::unique_ptr<StringPairVector> headers) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnPushPromise";
    wrapped_->OnPushPromise(std::move(frame), std::move(headers));
  }

  void OnRstStream(std::unique_ptr<SpdyRstStreamIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnRstStream";
    wrapped_->OnRstStream(std::move(frame));
  }

  // SpdySettingsIR has a map for settings, so loses info about the order of
  // settings, and whether the same setting appeared more than once, so the
  // the actual settings (parameter and value) are provided in a vector.
  void OnSettings(std::unique_ptr<SpdySettingsIR> frame,
                  std::unique_ptr<SettingVector> settings) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnSettings";
    wrapped_->OnSettings(std::move(frame), std::move(settings));
  }

  // A settings frame with an ACK has no content, but for uniformity passing
  // a frame with the ACK flag set.
  void OnSettingsAck(std::unique_ptr<SpdySettingsIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnSettingsAck";
    wrapped_->OnSettingsAck(std::move(frame));
  }

  void OnWindowUpdate(std::unique_ptr<SpdyWindowUpdateIR> frame) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnWindowUpdate";
    wrapped_->OnWindowUpdate(std::move(frame));
  }

  // The SpdyFramer will not process any more data at this point.
  void OnError(http2::Http2DecoderAdapter::SpdyFramerError error,
               SpdyTestDeframer* deframer) override {
    DVLOG(1) << "LoggingSpdyDeframerDelegate::OnError";
    wrapped_->OnError(error, deframer);
  }

 private:
  std::unique_ptr<SpdyDeframerVisitorInterface> wrapped_;
};

// static
std::unique_ptr<SpdyDeframerVisitorInterface>
SpdyDeframerVisitorInterface::LogBeforeVisiting(
    std::unique_ptr<SpdyDeframerVisitorInterface> wrapped_listener) {
  return SpdyMakeUnique<LoggingSpdyDeframerDelegate>(
      std::move(wrapped_listener));
}

CollectedFrame::CollectedFrame() = default;

CollectedFrame::CollectedFrame(CollectedFrame&& other)
    : frame_ir(std::move(other.frame_ir)),
      headers(std::move(other.headers)),
      settings(std::move(other.settings)),
      error_reported(other.error_reported) {}

CollectedFrame::~CollectedFrame() = default;

CollectedFrame& CollectedFrame::operator=(CollectedFrame&& other) {
  frame_ir = std::move(other.frame_ir);
  headers = std::move(other.headers);
  settings = std::move(other.settings);
  error_reported = other.error_reported;
  return *this;
}

::testing::AssertionResult CollectedFrame::VerifyHasHeaders(
    const StringPairVector& expected_headers) const {
  VERIFY_NE(headers.get(), nullptr);
  VERIFY_THAT(*headers, ::testing::ContainerEq(expected_headers));
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult CollectedFrame::VerifyHasSettings(
    const SettingVector& expected_settings) const {
  VERIFY_NE(settings.get(), nullptr);
  VERIFY_THAT(*settings, testing::ContainerEq(expected_settings));
  return ::testing::AssertionSuccess();
}

DeframerCallbackCollector::DeframerCallbackCollector(
    std::vector<CollectedFrame>* collected_frames)
    : collected_frames_(collected_frames) {
  CHECK(collected_frames);
}

void DeframerCallbackCollector::OnAltSvc(
    std::unique_ptr<SpdyAltSvcIR> frame_ir) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}
void DeframerCallbackCollector::OnData(std::unique_ptr<SpdyDataIR> frame_ir) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}
void DeframerCallbackCollector::OnGoAway(
    std::unique_ptr<SpdyGoAwayIR> frame_ir) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}

// SpdyHeadersIR and SpdyPushPromiseIR each has a SpdyHeaderBlock which
// significantly modifies the headers, so the actual header entries (name
// and value strings) are provided in a vector.
void DeframerCallbackCollector::OnHeaders(
    std::unique_ptr<SpdyHeadersIR> frame_ir,
    std::unique_ptr<StringPairVector> headers) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  cf.headers = std::move(headers);
  collected_frames_->push_back(std::move(cf));
}

void DeframerCallbackCollector::OnPing(std::unique_ptr<SpdyPingIR> frame_ir) {
  EXPECT_TRUE(frame_ir && !frame_ir->is_ack());
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}

void DeframerCallbackCollector::OnPingAck(
    std::unique_ptr<SpdyPingIR> frame_ir) {
  EXPECT_TRUE(frame_ir && frame_ir->is_ack());
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}

void DeframerCallbackCollector::OnPriority(
    std::unique_ptr<SpdyPriorityIR> frame_ir) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}

// SpdyHeadersIR and SpdyPushPromiseIR each has a SpdyHeaderBlock which
// significantly modifies the headers, so the actual header entries (name
// and value strings) are provided in a vector.
void DeframerCallbackCollector::OnPushPromise(
    std::unique_ptr<SpdyPushPromiseIR> frame_ir,
    std::unique_ptr<StringPairVector> headers) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  cf.headers = std::move(headers);
  collected_frames_->push_back(std::move(cf));
}

void DeframerCallbackCollector::OnRstStream(
    std::unique_ptr<SpdyRstStreamIR> frame_ir) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}

// SpdySettingsIR has a map for settings, so loses info about the order of
// settings, and whether the same setting appeared more than once, so the
// the actual settings (parameter and value) are provided in a vector.
void DeframerCallbackCollector::OnSettings(
    std::unique_ptr<SpdySettingsIR> frame_ir,
    std::unique_ptr<SettingVector> settings) {
  EXPECT_TRUE(frame_ir && !frame_ir->is_ack());
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  cf.settings = std::move(settings);
  collected_frames_->push_back(std::move(cf));
}

// A settings frame_ir with an ACK has no content, but for uniformity passing
// a frame_ir with the ACK flag set.
void DeframerCallbackCollector::OnSettingsAck(
    std::unique_ptr<SpdySettingsIR> frame_ir) {
  EXPECT_TRUE(frame_ir && frame_ir->is_ack());
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}

void DeframerCallbackCollector::OnWindowUpdate(
    std::unique_ptr<SpdyWindowUpdateIR> frame_ir) {
  CollectedFrame cf;
  cf.frame_ir = std::move(frame_ir);
  collected_frames_->push_back(std::move(cf));
}

// The SpdyFramer will not process any more data at this point.
void DeframerCallbackCollector::OnError(
    http2::Http2DecoderAdapter::SpdyFramerError error,
    SpdyTestDeframer* deframer) {
  CollectedFrame cf;
  cf.error_reported = true;
  collected_frames_->push_back(std::move(cf));
}

}  // namespace test
}  // namespace spdy
