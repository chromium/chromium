// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/buffered_spdy_framer.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "net/log/net_log_with_source.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class TestBufferedSpdyVisitor : public BufferedSpdyFramerVisitorInterface {
 public:
  TestBufferedSpdyVisitor()
      : buffered_spdy_framer_(kMaxHeaderListSizeForTest, NetLogWithSource()),
        header_stream_id_(static_cast<spdy::SpdyStreamId>(-1)),
        promised_stream_id_(static_cast<spdy::SpdyStreamId>(-1)) {}

  void OnError(
      http2::Http2DecoderAdapter::SpdyFramerError spdy_framer_error) override {
    VLOG(1) << "spdy::SpdyFramer Error: " << spdy_framer_error;
    error_count_++;
  }

  void OnStreamError(spdy::SpdyStreamId stream_id,
                     const std::string& description) override {
    VLOG(1) << "spdy::SpdyFramer Error on stream: " << stream_id << " "
            << description;
    error_count_++;
  }

  void OnHeaders(spdy::SpdyStreamId stream_id,
                 bool has_priority,
                 int weight,
                 spdy::SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 quiche::HttpHeaderBlock headers,
                 base::TimeTicks recv_first_byte_time) override {
    header_stream_id_ = stream_id;
    headers_frame_count_++;
    headers_ = std::move(headers);
  }

  void OnDataFrameHeader(spdy::SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override {
    ADD_FAILURE() << "Unexpected OnDataFrameHeader call.";
  }

  void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override {
    LOG(FATAL) << "Unexpected OnStreamFrameData call.";
  }

  void OnStreamEnd(spdy::SpdyStreamId stream_id) override {
    LOG(FATAL) << "Unexpected OnStreamEnd call.";
  }

  void OnStreamPadding(spdy::SpdyStreamId stream_id, size_t len) override {
    LOG(FATAL) << "Unexpected OnStreamPadding call.";
  }

  void OnSettings() override {}

  void OnSettingsAck() override {}

  void OnSettingsEnd() override {}

  void OnSetting(spdy::SpdySettingsId id, uint32_t value) override {
    setting_count_++;
  }

  void OnPing(spdy::SpdyPingId unique_id, bool is_ack) override {}

  void OnRstStream(spdy::SpdyStreamId stream_id,
                   spdy::SpdyErrorCode error_code) override {}

  void OnGoAway(spdy::SpdyStreamId last_accepted_stream_id,
                spdy::SpdyErrorCode error_code,
                std::string_view debug_data) override {
    goaway_count_++;
    goaway_last_accepted_stream_id_ = last_accepted_stream_id;
    goaway_error_code_ = error_code;
    goaway_debug_data_.assign(debug_data.data(), debug_data.size());
  }

  void OnDataFrameHeader(const spdy::SpdySerializedFrame* frame) {
    LOG(FATAL) << "Unexpected OnDataFrameHeader call.";
  }

  void OnRstStream(const spdy::SpdySerializedFrame& frame) {}
  void OnGoAway(const spdy::SpdySerializedFrame& frame) {}
  void OnPing(const spdy::SpdySerializedFrame& frame) {}
  void OnWindowUpdate(spdy::SpdyStreamId stream_id,
                      int delta_window_size) override {}

  void OnPushPromise(spdy::SpdyStreamId stream_id,
                     spdy::SpdyStreamId promised_stream_id,
                     quiche::HttpHeaderBlock headers) override {
    header_stream_id_ = stream_id;
    push_promise_frame_count_++;
    promised_stream_id_ = promised_stream_id;
    headers_ = std::move(headers);
  }

  void OnAltSvc(spdy::SpdyStreamId stream_id,
                std::string_view origin,
                const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector&
                    altsvc_vector) override {
    altsvc_count_++;
    altsvc_stream_id_ = stream_id;
    altsvc_origin_.assign(origin.data(), origin.size());
    altsvc_vector_ = altsvc_vector;
  }

  bool OnUnknownFrame(spdy::SpdyStreamId stream_id,
                      uint8_t frame_type) override {
    return true;
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const spdy::SpdySerializedFrame& frame) {
    const char* input_ptr = frame.data();
    size_t input_remaining = frame.size();
    buffered_spdy_framer_.set_visitor(this);
    while (input_remaining > 0 &&
           buffered_spdy_framer_.spdy_framer_error() ==
               http2::Http2DecoderAdapter::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % std::min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed =
          buffered_spdy_framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
    }
  }

  BufferedSpdyFramer buffered_spdy_framer_;

  // Counters from the visitor callbacks.
  int error_count_ = 0;
  int setting_count_ = 0;
  int headers_frame_count_ = 0;
  int push_promise_frame_count_ = 0;
  int goaway_count_ = 0;
  int altsvc_count_ = 0;

  // Header block streaming state:
  spdy::SpdyStreamId header_stream_id_;
  spdy::SpdyStreamId promised_stream_id_;

  // Headers from OnHeaders and OnPushPromise for verification.
  quiche::HttpHeaderBlock headers_;

  // OnGoAway parameters.
  spdy::SpdyStreamId goaway_last_accepted_stream_id_;
  spdy::SpdyErrorCode goaway_error_code_;
  std::string goaway_debug_data_;

  // OnAltSvc parameters.
  spdy::SpdyStreamId altsvc_stream_id_;
  std::string altsvc_origin_;
  spdy::SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector_;
};

}  // namespace

class BufferedSpdyFramerTest : public PlatformTest {};

TEST_F(BufferedSpdyFramerTest, OnSetting) {
  spdy::SpdyFramer framer(spdy::SpdyFramer::ENABLE_COMPRESSION);
  spdy::SpdySettingsIR settings_ir;
  settings_ir.AddSetting(spdy::SETTINGS_INITIAL_WINDOW_SIZE, 2);
  settings_ir.AddSetting(spdy::SETTINGS_MAX_CONCURRENT_STREAMS, 3);
  spdy::SpdySerializedFrame control_frame(
      framer.SerializeSettings(settings_ir));
  TestBufferedSpdyVisitor visitor;

  visitor.SimulateInFramer(control_frame);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.setting_count_);
}

TEST_F(BufferedSpdyFramerTest, HeaderListTooLarge) {
  quiche::HttpHeaderBlock headers;
  std::string long_header_value(256 * 1024, 'x');
  headers["foo"] = long_header_value;
  spdy::SpdyHeadersIR headers_ir(/*stream_id=*/1, std::move(headers));

  NetLogWithSource net_log;
  BufferedSpdyFramer framer(kMaxHeaderListSizeForTest, net_log);
  spdy::SpdySerializedFrame control_frame = framer.SerializeFrame(headers_ir);

  TestBufferedSpdyVisitor visitor;
  visitor.SimulateInFramer(control_frame);

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  EXPECT_EQ(quiche::HttpHeaderBlock(), visitor.headers_);
}

TEST_F(BufferedSpdyFramerTest, ValidHeadersAfterInvalidHeaders) {
  quiche::HttpHeaderBlock headers;
  headers["invalid"] = "\r\n\r\n";

  quiche::HttpHeaderBlock headers2;
  headers["alpha"] = "beta";

  SpdyTestUtil spdy_test_util;
  spdy::SpdySerializedFrame headers_frame(
      spdy_test_util.ConstructSpdyReply(1, std::move(headers)));
  spdy::SpdySerializedFrame headers_frame2(
      spdy_test_util.ConstructSpdyReply(2, std::move(headers2)));

  TestBufferedSpdyVisitor visitor;
  visitor.SimulateInFramer(headers_frame);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);

  visitor.SimulateInFramer(headers_frame2);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
}

TEST_F(BufferedSpdyFramerTest, ReadHeadersHeaderBlock) {
  quiche::HttpHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  spdy::SpdyHeadersIR headers_ir(/*stream_id=*/1, headers.Clone());

  NetLogWithSource net_log;
  BufferedSpdyFramer framer(kMaxHeaderListSizeForTest, net_log);
  spdy::SpdySerializedFrame control_frame = framer.SerializeFrame(headers_ir);

  TestBufferedSpdyVisitor visitor;
  visitor.SimulateInFramer(control_frame);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  EXPECT_EQ(headers, visitor.headers_);
}

TEST_F(BufferedSpdyFramerTest, ReadPushPromiseHeaderBlock) {
  quiche::HttpHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  NetLogWithSource net_log;
  BufferedSpdyFramer framer(kMaxHeaderListSizeForTest, net_log);
  spdy::SpdyPushPromiseIR push_promise_ir(
      /*stream_id=*/1, /*promised_stream_id=*/2, headers.Clone());
  spdy::SpdySerializedFrame control_frame =
      framer.SerializeFrame(push_promise_ir);

  TestBufferedSpdyVisitor visitor;
  visitor.SimulateInFramer(control_frame);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_EQ(headers, visitor.headers_);
  EXPECT_EQ(1u, visitor.header_stream_id_);
  EXPECT_EQ(2u, visitor.promised_stream_id_);
}

TEST_F(BufferedSpdyFramerTest, GoAwayDebugData) {
  spdy::SpdyGoAwayIR go_ir(/*last_good_stream_id=*/2,
                           spdy::ERROR_CODE_FRAME_SIZE_ERROR, "foo");
  NetLogWithSource net_log;
  BufferedSpdyFramer framer(kMaxHeaderListSizeForTest, net_log);
  spdy::SpdySerializedFrame goaway_frame = framer.SerializeFrame(go_ir);

  TestBufferedSpdyVisitor visitor;
  visitor.SimulateInFramer(goaway_frame);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.goaway_count_);
  EXPECT_EQ(2u, visitor.goaway_last_accepted_stream_id_);
  EXPECT_EQ(spdy::ERROR_CODE_FRAME_SIZE_ERROR, visitor.goaway_error_code_);
  EXPECT_EQ("foo", visitor.goaway_debug_data_);
}

// ALTSVC frame on stream 0 must have an origin.
TEST_F(BufferedSpdyFramerTest, OnAltSvcOnStreamZero) {
  const spdy::SpdyStreamId altsvc_stream_id(0);
  spdy::SpdyAltSvcIR altsvc_ir(altsvc_stream_id);
  spdy::SpdyAltSvcWireFormat::AlternativeService alternative_service(
      "quic", "alternative.example.org", 443, 86400,
      spdy::SpdyAltSvcWireFormat::VersionVector());
  altsvc_ir.add_altsvc(alternative_service);
  const char altsvc_origin[] = "https://www.example.org";
  altsvc_ir.set_origin(altsvc_origin);
  NetLogWithSource net_log;
  BufferedSpdyFramer framer(kMaxHeaderListSizeForTest, net_log);
  spdy::SpdySerializedFrame altsvc_frame(framer.SerializeFrame(altsvc_ir));

  TestBufferedSpdyVisitor visitor;
  visitor.SimulateInFramer(altsvc_frame);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.altsvc_count_);
  EXPECT_EQ(altsvc_stream_id, visitor.altsvc_stream_id_);
  EXPECT_EQ(altsvc_origin, visitor.altsvc_origin_);
  ASSERT_EQ(1u, visitor.altsvc_vector_.size());
  EXPECT_EQ(alternative_service, visitor.altsvc_vector_[0]);
}

// ALTSVC frame on a non-zero stream must not have an origin.
TEST_F(BufferedSpdyFramerTest, OnAltSvcOnNonzeroStream) {
  const spdy::SpdyStreamId altsvc_stream_id(1);
  spdy::SpdyAltSvcIR altsvc_ir(altsvc_stream_id);
  spdy::SpdyAltSvcWireFormat::AlternativeService alternative_service(
      "quic", "alternative.example.org", 443, 86400,
      spdy::SpdyAltSvcWireFormat::VersionVector());
  altsvc_ir.add_altsvc(alternative_service);
  NetLogWithSource net_log;
  BufferedSpdyFramer framer(kMaxHeaderListSizeForTest, net_log);
  spdy::SpdySerializedFrame altsvc_frame(framer.SerializeFrame(altsvc_ir));

  TestBufferedSpdyVisitor visitor;
  visitor.SimulateInFramer(altsvc_frame);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.altsvc_count_);
  EXPECT_EQ(altsvc_stream_id, visitor.altsvc_stream_id_);
  EXPECT_TRUE(visitor.altsvc_origin_.empty());
  ASSERT_EQ(1u, visitor.altsvc_vector_.size());
  EXPECT_EQ(alternative_service, visitor.altsvc_vector_[0]);
}

}  // namespace net
