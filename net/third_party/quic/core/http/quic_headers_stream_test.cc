// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_headers_stream.h"

#include <cstdint>
#include <ostream>
#include <tuple>
#include <utility>
#include <vector>

#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/spdy/core/http2_frame_decoder_adapter.h"
#include "net/third_party/spdy/core/spdy_alt_svc_wire_format.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"

using spdy::ERROR_CODE_PROTOCOL_ERROR;
using spdy::SETTINGS_ENABLE_PUSH;
using spdy::SETTINGS_HEADER_TABLE_SIZE;
using spdy::SETTINGS_INITIAL_WINDOW_SIZE;
using spdy::SETTINGS_MAX_CONCURRENT_STREAMS;
using spdy::SETTINGS_MAX_FRAME_SIZE;
using spdy::SETTINGS_MAX_HEADER_LIST_SIZE;
using spdy::Spdy3PriorityToHttp2Weight;
using spdy::SpdyAltSvcWireFormat;
using spdy::SpdyDataIR;
using spdy::SpdyErrorCode;
using spdy::SpdyFramer;
using spdy::SpdyFramerVisitorInterface;
using spdy::SpdyGoAwayIR;
using spdy::SpdyHeaderBlock;
using spdy::SpdyHeadersHandlerInterface;
using spdy::SpdyHeadersIR;
using spdy::SpdyKnownSettingsId;
using spdy::SpdyPingId;
using spdy::SpdyPingIR;
using spdy::SpdyPriority;
using spdy::SpdyPriorityIR;
using spdy::SpdyPushPromiseIR;
using spdy::SpdyRstStreamIR;
using spdy::SpdySerializedFrame;
using spdy::SpdySettingsId;
using spdy::SpdySettingsIR;
using spdy::SpdyStreamId;
using spdy::SpdyWindowUpdateIR;
using spdy::test::TestHeadersHandler;
using testing::_;
using testing::AtLeast;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace quic {
namespace test {

class MockQuicHpackDebugVisitor : public QuicHpackDebugVisitor {
 public:
  MockQuicHpackDebugVisitor() : QuicHpackDebugVisitor() {}
  MockQuicHpackDebugVisitor(const MockQuicHpackDebugVisitor&) = delete;
  MockQuicHpackDebugVisitor& operator=(const MockQuicHpackDebugVisitor&) =
      delete;

  MOCK_METHOD1(OnUseEntry, void(QuicTime::Delta elapsed));
};

namespace {

class MockVisitor : public SpdyFramerVisitorInterface {
 public:
  MOCK_METHOD1(OnError,
               void(http2::Http2DecoderAdapter::SpdyFramerError error));
  MOCK_METHOD3(OnDataFrameHeader,
               void(SpdyStreamId stream_id, size_t length, bool fin));
  MOCK_METHOD3(OnStreamFrameData,
               void(SpdyStreamId stream_id, const char* data, size_t len));
  MOCK_METHOD1(OnStreamEnd, void(SpdyStreamId stream_id));
  MOCK_METHOD2(OnStreamPadding, void(SpdyStreamId stream_id, size_t len));
  MOCK_METHOD1(OnHeaderFrameStart,
               SpdyHeadersHandlerInterface*(SpdyStreamId stream_id));
  MOCK_METHOD1(OnHeaderFrameEnd, void(SpdyStreamId stream_id));
  MOCK_METHOD3(OnControlFrameHeaderData,
               bool(SpdyStreamId stream_id,
                    const char* header_data,
                    size_t len));
  MOCK_METHOD2(OnRstStream,
               void(SpdyStreamId stream_id, SpdyErrorCode error_code));
  MOCK_METHOD0(OnSettings, void());
  MOCK_METHOD2(OnSetting, void(SpdySettingsId id, uint32_t value));
  MOCK_METHOD0(OnSettingsAck, void());
  MOCK_METHOD0(OnSettingsEnd, void());
  MOCK_METHOD2(OnPing, void(SpdyPingId unique_id, bool is_ack));
  MOCK_METHOD2(OnGoAway,
               void(SpdyStreamId last_accepted_stream_id,
                    SpdyErrorCode error_code));
  MOCK_METHOD7(OnHeaders,
               void(SpdyStreamId stream_id,
                    bool has_priority,
                    int weight,
                    SpdyStreamId parent_stream_id,
                    bool exclusive,
                    bool fin,
                    bool end));
  MOCK_METHOD2(OnWindowUpdate,
               void(SpdyStreamId stream_id, int delta_window_size));
  MOCK_METHOD1(OnBlocked, void(SpdyStreamId stream_id));
  MOCK_METHOD3(OnPushPromise,
               void(SpdyStreamId stream_id,
                    SpdyStreamId promised_stream_id,
                    bool end));
  MOCK_METHOD2(OnContinuation, void(SpdyStreamId stream_id, bool end));
  MOCK_METHOD3(OnAltSvc,
               void(SpdyStreamId stream_id,
                    QuicStringPiece origin,
                    const SpdyAltSvcWireFormat::AlternativeServiceVector&
                        altsvc_vector));
  MOCK_METHOD4(OnPriority,
               void(SpdyStreamId stream_id,
                    SpdyStreamId parent_stream_id,
                    int weight,
                    bool exclusive));
  MOCK_METHOD2(OnUnknownFrame,
               bool(SpdyStreamId stream_id, uint8_t frame_type));
};

struct TestParams {
  TestParams(const ParsedQuicVersion& version, Perspective perspective)
      : version(version), perspective(perspective) {
    QUIC_LOG(INFO) << "TestParams: version: "
                   << ParsedQuicVersionToString(version)
                   << ", perspective: " << perspective;
  }

  TestParams(const TestParams& other)
      : version(other.version), perspective(other.perspective) {}

  ParsedQuicVersion version;
  Perspective perspective;
};

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  ParsedQuicVersionVector all_supported_versions = AllSupportedVersions();
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    for (Perspective p : {Perspective::IS_SERVER, Perspective::IS_CLIENT}) {
      params.emplace_back(all_supported_versions[i], p);
    }
  }
  return params;
}

class QuicHeadersStreamTest : public QuicTestWithParam<TestParams> {
 public:
  QuicHeadersStreamTest()
      : connection_(new StrictMock<MockQuicConnection>(&helper_,
                                                       &alarm_factory_,
                                                       perspective(),
                                                       GetVersion())),
        session_(connection_),
        body_("hello world"),
        stream_frame_(
            QuicUtils::GetHeadersStreamId(connection_->transport_version()),
            /*fin=*/false,
            /*offset=*/0,
            ""),
        next_promised_stream_id_(2) {
    session_.Initialize();
    headers_stream_ = QuicSpdySessionPeer::GetHeadersStream(&session_);
    headers_[":version"] = "HTTP/1.1";
    headers_[":status"] = "200 Ok";
    headers_["content-length"] = "11";
    framer_ = std::unique_ptr<SpdyFramer>(
        new SpdyFramer(SpdyFramer::ENABLE_COMPRESSION));
    deframer_ = std::unique_ptr<http2::Http2DecoderAdapter>(
        new http2::Http2DecoderAdapter());
    deframer_->set_visitor(&visitor_);
    EXPECT_EQ(transport_version(), session_.connection()->transport_version());
    EXPECT_TRUE(headers_stream_ != nullptr);
    connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
    client_id_1_ =
        QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0);
    client_id_2_ =
        QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 1);
    client_id_3_ =
        QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 2);
    next_stream_id_ = QuicSpdySessionPeer::NextStreamId(session_);
  }

  QuicStreamId GetNthClientInitiatedId(int n) {
    return QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, n);
  }

  QuicConsumedData SaveIov(size_t write_length) {
    char* buf = new char[write_length];
    QuicDataWriter writer(write_length, buf, NETWORK_BYTE_ORDER);
    headers_stream_->WriteStreamData(headers_stream_->stream_bytes_written(),
                                     write_length, &writer);
    saved_data_.append(buf, write_length);
    delete[] buf;
    return QuicConsumedData(write_length, false);
  }

  void SavePayload(const char* data, size_t len) {
    saved_payloads_.append(data, len);
  }

  bool SaveHeaderData(const char* data, int len) {
    saved_header_data_.append(data, len);
    return true;
  }

  void SaveHeaderDataStringPiece(QuicStringPiece data) {
    saved_header_data_.append(data.data(), data.length());
  }

  void SavePromiseHeaderList(QuicStreamId /* stream_id */,
                             QuicStreamId /* promised_stream_id */,
                             size_t size,
                             const QuicHeaderList& header_list) {
    SaveToHandler(size, header_list);
  }

  void SaveHeaderList(QuicStreamId /* stream_id */,
                      bool /* fin */,
                      size_t size,
                      const QuicHeaderList& header_list) {
    SaveToHandler(size, header_list);
  }

  void SaveToHandler(size_t size, const QuicHeaderList& header_list) {
    headers_handler_ = QuicMakeUnique<TestHeadersHandler>();
    headers_handler_->OnHeaderBlockStart();
    for (const auto& p : header_list) {
      headers_handler_->OnHeader(p.first, p.second);
    }
    headers_handler_->OnHeaderBlockEnd(size, size);
  }

  void WriteAndExpectRequestHeaders(QuicStreamId stream_id,
                                    bool fin,
                                    SpdyPriority priority) {
    WriteHeadersAndCheckData(stream_id, fin, priority, true /*is_request*/);
  }

  void WriteAndExpectResponseHeaders(QuicStreamId stream_id, bool fin) {
    WriteHeadersAndCheckData(stream_id, fin, 0, false /*is_request*/);
  }

  void WriteHeadersAndCheckData(QuicStreamId stream_id,
                                bool fin,
                                SpdyPriority priority,
                                bool is_request) {
    // Write the headers and capture the outgoing data
    EXPECT_CALL(session_, WritevData(headers_stream_,
                                     QuicUtils::GetHeadersStreamId(
                                         connection_->transport_version()),
                                     _, _, NO_FIN))
        .WillOnce(WithArgs<2>(Invoke(this, &QuicHeadersStreamTest::SaveIov)));
    QuicSpdySessionPeer::WriteHeadersImpl(
        &session_, stream_id, headers_.Clone(), fin,
        Spdy3PriorityToHttp2Weight(priority), 0, false, nullptr);

    // Parse the outgoing data and check that it matches was was written.
    if (is_request) {
      EXPECT_CALL(visitor_,
                  OnHeaders(stream_id, kHasPriority,
                            Spdy3PriorityToHttp2Weight(priority),
                            /*parent_stream_id=*/0,
                            /*exclusive=*/false, fin, kFrameComplete));
    } else {
      EXPECT_CALL(visitor_,
                  OnHeaders(stream_id, !kHasPriority,
                            /*priority=*/0,
                            /*parent_stream_id=*/0,
                            /*exclusive=*/false, fin, kFrameComplete));
    }
    headers_handler_ = QuicMakeUnique<TestHeadersHandler>();
    EXPECT_CALL(visitor_, OnHeaderFrameStart(stream_id))
        .WillOnce(Return(headers_handler_.get()));
    EXPECT_CALL(visitor_, OnHeaderFrameEnd(stream_id)).Times(1);
    if (fin) {
      EXPECT_CALL(visitor_, OnStreamEnd(stream_id));
    }
    deframer_->ProcessInput(saved_data_.data(), saved_data_.length());
    EXPECT_FALSE(deframer_->HasError())
        << http2::Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer_->spdy_framer_error());

    CheckHeaders();
    saved_data_.clear();
  }

  void CheckHeaders() {
    EXPECT_EQ(headers_, headers_handler_->decoded_block());
    headers_handler_.reset();
  }

  Perspective perspective() const { return GetParam().perspective; }

  QuicTransportVersion transport_version() const {
    return GetParam().version.transport_version;
  }

  ParsedQuicVersionVector GetVersion() {
    ParsedQuicVersionVector versions;
    versions.push_back(GetParam().version);
    return versions;
  }

  void TearDownLocalConnectionState() {
    QuicConnectionPeer::TearDownLocalConnectionState(connection_);
  }

  QuicStreamId NextPromisedStreamId() {
    return next_promised_stream_id_ += next_stream_id_;
  }

  static const bool kFrameComplete = true;
  static const bool kHasPriority = true;

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  StrictMock<MockQuicSpdySession> session_;
  QuicHeadersStream* headers_stream_;
  SpdyHeaderBlock headers_;
  std::unique_ptr<TestHeadersHandler> headers_handler_;
  QuicString body_;
  QuicString saved_data_;
  QuicString saved_header_data_;
  QuicString saved_payloads_;
  std::unique_ptr<SpdyFramer> framer_;
  std::unique_ptr<http2::Http2DecoderAdapter> deframer_;
  StrictMock<MockVisitor> visitor_;
  QuicStreamFrame stream_frame_;
  QuicStreamId next_promised_stream_id_;
  QuicStreamId client_id_1_;
  QuicStreamId client_id_2_;
  QuicStreamId client_id_3_;
  QuicStreamId next_stream_id_;
};

// Run all tests with each version and perspective (client or server).
INSTANTIATE_TEST_CASE_P(Tests,
                        QuicHeadersStreamTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicHeadersStreamTest, StreamId) {
  EXPECT_EQ(QuicUtils::GetHeadersStreamId(connection_->transport_version()),
            headers_stream_->id());
}

TEST_P(QuicHeadersStreamTest, WriteHeaders) {
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      if (perspective() == Perspective::IS_SERVER) {
        WriteAndExpectResponseHeaders(stream_id, fin);
      } else {
        for (SpdyPriority priority = 0; priority < 7; ++priority) {
          // TODO(rch): implement priorities correctly.
          WriteAndExpectRequestHeaders(stream_id, fin, 0);
        }
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, WritePushPromises) {
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    QuicStreamId promised_stream_id = NextPromisedStreamId();
    if (perspective() == Perspective::IS_SERVER) {
      // Write the headers and capture the outgoing data
      EXPECT_CALL(session_, WritevData(headers_stream_,
                                       QuicUtils::GetHeadersStreamId(
                                           connection_->transport_version()),
                                       _, _, NO_FIN))
          .WillOnce(WithArgs<2>(Invoke(this, &QuicHeadersStreamTest::SaveIov)));
      session_.WritePushPromise(stream_id, promised_stream_id,
                                headers_.Clone());

      // Parse the outgoing data and check that it matches was was written.
      EXPECT_CALL(visitor_,
                  OnPushPromise(stream_id, promised_stream_id, kFrameComplete));
      headers_handler_ = QuicMakeUnique<TestHeadersHandler>();
      EXPECT_CALL(visitor_, OnHeaderFrameStart(stream_id))
          .WillOnce(Return(headers_handler_.get()));
      EXPECT_CALL(visitor_, OnHeaderFrameEnd(stream_id)).Times(1);
      deframer_->ProcessInput(saved_data_.data(), saved_data_.length());
      EXPECT_FALSE(deframer_->HasError())
          << http2::Http2DecoderAdapter::SpdyFramerErrorToString(
                 deframer_->spdy_framer_error());
      CheckHeaders();
      saved_data_.clear();
    } else {
      EXPECT_QUIC_BUG(session_.WritePushPromise(stream_id, promised_stream_id,
                                                headers_.Clone()),
                      "Client shouldn't send PUSH_PROMISE");
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessRawData) {
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      for (SpdyPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        SpdySerializedFrame frame;
        if (perspective() == Perspective::IS_SERVER) {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          headers_frame.set_has_priority(true);
          headers_frame.set_weight(Spdy3PriorityToHttp2Weight(0));
          frame = framer_->SerializeFrame(headers_frame);
          EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0));
        } else {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          frame = framer_->SerializeFrame(headers_frame);
        }
        EXPECT_CALL(session_,
                    OnStreamHeaderList(stream_id, fin, frame.size(), _))
            .WillOnce(Invoke(this, &QuicHeadersStreamTest::SaveHeaderList));
        stream_frame_.data_buffer = frame.data();
        stream_frame_.data_length = frame.size();
        headers_stream_->OnStreamFrame(stream_frame_);
        stream_frame_.offset += frame.size();
        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessPushPromise) {
  if (perspective() == Perspective::IS_SERVER) {
    return;
  }
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    QuicStreamId promised_stream_id = NextPromisedStreamId();
    SpdyPushPromiseIR push_promise(stream_id, promised_stream_id,
                                   headers_.Clone());
    SpdySerializedFrame frame(framer_->SerializeFrame(push_promise));
    if (perspective() == Perspective::IS_SERVER) {
      EXPECT_CALL(*connection_,
                  CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                  "PUSH_PROMISE not supported.", _))
          .WillRepeatedly(InvokeWithoutArgs(
              this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
    } else {
      EXPECT_CALL(session_, OnPromiseHeaderList(stream_id, promised_stream_id,
                                                frame.size(), _))
          .WillOnce(
              Invoke(this, &QuicHeadersStreamTest::SavePromiseHeaderList));
    }
    stream_frame_.data_buffer = frame.data();
    stream_frame_.data_length = frame.size();
    headers_stream_->OnStreamFrame(stream_frame_);
    if (perspective() == Perspective::IS_CLIENT) {
      stream_frame_.offset += frame.size();
      CheckHeaders();
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessPriorityFrame) {
  QuicStreamId parent_stream_id = 0;
  for (SpdyPriority priority = 0; priority < 7; ++priority) {
    for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
         stream_id += next_stream_id_) {
      int weight = Spdy3PriorityToHttp2Weight(priority);
      SpdyPriorityIR priority_frame(stream_id, parent_stream_id, weight, true);
      SpdySerializedFrame frame(framer_->SerializeFrame(priority_frame));
      parent_stream_id = stream_id;
      if (transport_version() <= QUIC_VERSION_39) {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                    "SPDY PRIORITY frame received.", _))
            .WillRepeatedly(InvokeWithoutArgs(
                this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
      } else if (perspective() == Perspective::IS_CLIENT) {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                    "Server must not send PRIORITY frames.", _))
            .WillRepeatedly(InvokeWithoutArgs(
                this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
      } else {
        EXPECT_CALL(session_, OnPriorityFrame(stream_id, priority)).Times(1);
      }
      stream_frame_.data_buffer = frame.data();
      stream_frame_.data_length = frame.size();
      headers_stream_->OnStreamFrame(stream_frame_);
      stream_frame_.offset += frame.size();
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessPushPromiseDisabledSetting) {
  session_.OnConfigNegotiated();
  SpdySettingsIR data;
  // Respect supported settings frames SETTINGS_ENABLE_PUSH.
  data.AddSetting(SETTINGS_ENABLE_PUSH, 0);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  if (perspective() == Perspective::IS_CLIENT) {
    EXPECT_CALL(
        *connection_,
        CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                        "Unsupported field of HTTP/2 SETTINGS frame: 2", _));
  }
  headers_stream_->OnStreamFrame(stream_frame_);
  EXPECT_EQ(session_.server_push_enabled(),
            perspective() == Perspective::IS_CLIENT);
}

TEST_P(QuicHeadersStreamTest, ProcessLargeRawData) {
  QuicSpdySessionPeer::SetMaxUncompressedHeaderBytes(&session_, 256 * 1024);
  // We want to create a frame that is more than the SPDY Framer's max control
  // frame size, which is 16K, but less than the HPACK decoders max decode
  // buffer size, which is 32K.
  headers_["key0"] = QuicString(1 << 13, '.');
  headers_["key1"] = QuicString(1 << 13, '.');
  headers_["key2"] = QuicString(1 << 13, '.');
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      for (SpdyPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        SpdySerializedFrame frame;
        if (perspective() == Perspective::IS_SERVER) {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          headers_frame.set_has_priority(true);
          headers_frame.set_weight(Spdy3PriorityToHttp2Weight(0));
          frame = framer_->SerializeFrame(headers_frame);
          EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0));
        } else {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          frame = framer_->SerializeFrame(headers_frame);
        }
        EXPECT_CALL(session_,
                    OnStreamHeaderList(stream_id, fin, frame.size(), _))
            .WillOnce(Invoke(this, &QuicHeadersStreamTest::SaveHeaderList));
        stream_frame_.data_buffer = frame.data();
        stream_frame_.data_length = frame.size();
        headers_stream_->OnStreamFrame(stream_frame_);
        stream_frame_.offset += frame.size();
        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessBadData) {
  const char kBadData[] = "blah blah blah";
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA, _, _))
      .Times(::testing::AnyNumber());
  stream_frame_.data_buffer = kBadData;
  stream_frame_.data_length = strlen(kBadData);
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyDataFrame) {
  SpdyDataIR data(/* stream_id = */ 2, "ping");
  SpdySerializedFrame frame(framer_->SerializeFrame(data));

  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                            "SPDY DATA frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyRstStreamFrame) {
  SpdyRstStreamIR data(/* stream_id = */ 2, ERROR_CODE_PROTOCOL_ERROR);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                              "SPDY RST_STREAM frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, RespectHttp2SettingsFrameSupportedFields) {
  const uint32_t kTestHeaderTableSize = 1000;
  SpdySettingsIR data;
  // Respect supported settings frames SETTINGS_HEADER_TABLE_SIZE,
  // SETTINGS_MAX_HEADER_LIST_SIZE.
  data.AddSetting(SETTINGS_HEADER_TABLE_SIZE, kTestHeaderTableSize);
  data.AddSetting(SETTINGS_MAX_HEADER_LIST_SIZE, 2000);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
  EXPECT_EQ(kTestHeaderTableSize, QuicSpdySessionPeer::GetSpdyFramer(&session_)
                                      .header_encoder_table_size());
}

TEST_P(QuicHeadersStreamTest, RespectHttp2SettingsFrameUnsupportedFields) {
  SpdySettingsIR data;
  // Does not support SETTINGS_MAX_CONCURRENT_STREAMS,
  // SETTINGS_INITIAL_WINDOW_SIZE, SETTINGS_ENABLE_PUSH and
  // SETTINGS_MAX_FRAME_SIZE.
  data.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, 100);
  data.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, 100);
  data.AddSetting(SETTINGS_ENABLE_PUSH, 1);
  data.AddSetting(SETTINGS_MAX_FRAME_SIZE, 1250);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                      QuicStrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                                 SETTINGS_MAX_CONCURRENT_STREAMS),
                      _));
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                      QuicStrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                                 SETTINGS_INITIAL_WINDOW_SIZE),
                      _));
  if (session_.perspective() == Perspective::IS_CLIENT) {
    EXPECT_CALL(*connection_,
                CloseConnection(
                    QUIC_INVALID_HEADERS_STREAM_DATA,
                    QuicStrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                               SETTINGS_ENABLE_PUSH),
                    _));
  }
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                      QuicStrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                                 SETTINGS_MAX_FRAME_SIZE),
                      _));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyPingFrame) {
  SpdyPingIR data(1);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                            "SPDY PING frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyGoAwayFrame) {
  SpdyGoAwayIR data(/* last_good_stream_id = */ 1, ERROR_CODE_PROTOCOL_ERROR,
                    "go away");
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                            "SPDY GOAWAY frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyWindowUpdateFrame) {
  SpdyWindowUpdateIR data(/* stream_id = */ 1, /* delta = */ 1);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                              "SPDY WINDOW_UPDATE frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, NoConnectionLevelFlowControl) {
  EXPECT_FALSE(QuicStreamPeer::StreamContributesToConnectionFlowControl(
      headers_stream_));
}

TEST_P(QuicHeadersStreamTest, HpackDecoderDebugVisitor) {
  auto hpack_decoder_visitor =
      QuicMakeUnique<StrictMock<MockQuicHpackDebugVisitor>>();
  {
    InSequence seq;
    // Number of indexed representations generated in headers below.
    for (int i = 1; i < 28; i++) {
      EXPECT_CALL(*hpack_decoder_visitor,
                  OnUseEntry(QuicTime::Delta::FromMilliseconds(i)))
          .Times(4);
    }
  }
  QuicSpdySessionPeer::SetHpackDecoderDebugVisitor(
      &session_, std::move(hpack_decoder_visitor));

  // Create some headers we expect to generate entries in HPACK's
  // dynamic table, in addition to content-length.
  headers_["key0"] = QuicString(1 << 1, '.');
  headers_["key1"] = QuicString(1 << 2, '.');
  headers_["key2"] = QuicString(1 << 3, '.');
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      for (SpdyPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        SpdySerializedFrame frame;
        if (perspective() == Perspective::IS_SERVER) {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          headers_frame.set_has_priority(true);
          headers_frame.set_weight(Spdy3PriorityToHttp2Weight(0));
          frame = framer_->SerializeFrame(headers_frame);
          EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0));
        } else {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          frame = framer_->SerializeFrame(headers_frame);
        }
        EXPECT_CALL(session_,
                    OnStreamHeaderList(stream_id, fin, frame.size(), _))
            .WillOnce(Invoke(this, &QuicHeadersStreamTest::SaveHeaderList));
        stream_frame_.data_buffer = frame.data();
        stream_frame_.data_length = frame.size();
        connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
        headers_stream_->OnStreamFrame(stream_frame_);
        stream_frame_.offset += frame.size();
        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, HpackEncoderDebugVisitor) {
  auto hpack_encoder_visitor =
      QuicMakeUnique<StrictMock<MockQuicHpackDebugVisitor>>();
  if (perspective() == Perspective::IS_SERVER) {
    InSequence seq;
    for (int i = 1; i < 4; i++) {
      EXPECT_CALL(*hpack_encoder_visitor,
                  OnUseEntry(QuicTime::Delta::FromMilliseconds(i)));
    }
  } else {
    InSequence seq;
    for (int i = 1; i < 28; i++) {
      EXPECT_CALL(*hpack_encoder_visitor,
                  OnUseEntry(QuicTime::Delta::FromMilliseconds(i)));
    }
  }
  QuicSpdySessionPeer::SetHpackEncoderDebugVisitor(
      &session_, std::move(hpack_encoder_visitor));

  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      if (perspective() == Perspective::IS_SERVER) {
        WriteAndExpectResponseHeaders(stream_id, fin);
        connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
      } else {
        for (SpdyPriority priority = 0; priority < 7; ++priority) {
          // TODO(rch): implement priorities correctly.
          WriteAndExpectRequestHeaders(stream_id, fin, 0);
          connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
        }
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, AckSentData) {
  EXPECT_CALL(session_, WritevData(headers_stream_,
                                   QuicUtils::GetHeadersStreamId(
                                       connection_->transport_version()),
                                   _, _, NO_FIN))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  InSequence s;
  QuicReferenceCountedPointer<MockAckListener> ack_listener1(
      new MockAckListener());
  QuicReferenceCountedPointer<MockAckListener> ack_listener2(
      new MockAckListener());
  QuicReferenceCountedPointer<MockAckListener> ack_listener3(
      new MockAckListener());

  // Packet 1.
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);

  // Packet 2.
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);

  // Packet 3.
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);

  // Packet 2 gets retransmitted.
  EXPECT_CALL(*ack_listener3, OnPacketRetransmitted(7)).Times(1);
  EXPECT_CALL(*ack_listener2, OnPacketRetransmitted(7)).Times(1);
  headers_stream_->OnStreamFrameRetransmitted(21, 7, false);
  headers_stream_->OnStreamFrameRetransmitted(28, 7, false);

  // Packets are acked in order: 2, 3, 1.
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(21, 7, false,
                                                  QuicTime::Delta::Zero()));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(28, 7, false,
                                                  QuicTime::Delta::Zero()));

  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(35, 7, false,
                                                  QuicTime::Delta::Zero()));

  EXPECT_CALL(*ack_listener1, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener1, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(0, 7, false,
                                                  QuicTime::Delta::Zero()));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(7, 7, false,
                                                  QuicTime::Delta::Zero()));
  // Unsent data is acked.
  EXPECT_CALL(*ack_listener2, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(14, 10, false,
                                                  QuicTime::Delta::Zero()));
}

TEST_P(QuicHeadersStreamTest, FrameContainsMultipleHeaders) {
  // In this test, a stream frame can contain multiple headers.
  EXPECT_CALL(session_, WritevData(headers_stream_,
                                   QuicUtils::GetHeadersStreamId(
                                       connection_->transport_version()),
                                   _, _, NO_FIN))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  InSequence s;
  QuicReferenceCountedPointer<MockAckListener> ack_listener1(
      new MockAckListener());
  QuicReferenceCountedPointer<MockAckListener> ack_listener2(
      new MockAckListener());
  QuicReferenceCountedPointer<MockAckListener> ack_listener3(
      new MockAckListener());

  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);

  // Frame 1 is retransmitted.
  EXPECT_CALL(*ack_listener1, OnPacketRetransmitted(14));
  EXPECT_CALL(*ack_listener2, OnPacketRetransmitted(3));
  headers_stream_->OnStreamFrameRetransmitted(0, 17, false);

  // Frames are acked in order: 2, 3, 1.
  EXPECT_CALL(*ack_listener2, OnPacketAcked(4, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(2, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(17, 13, false,
                                                  QuicTime::Delta::Zero()));

  EXPECT_CALL(*ack_listener2, OnPacketAcked(5, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(30, 12, false,
                                                  QuicTime::Delta::Zero()));

  EXPECT_CALL(*ack_listener1, OnPacketAcked(14, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(3, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(0, 17, false,
                                                  QuicTime::Delta::Zero()));
}

TEST_P(QuicHeadersStreamTest, HeadersGetAckedMultipleTimes) {
  EXPECT_CALL(session_, WritevData(headers_stream_,
                                   QuicUtils::GetHeadersStreamId(
                                       connection_->transport_version()),
                                   _, _, NO_FIN))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  InSequence s;
  QuicReferenceCountedPointer<MockAckListener> ack_listener1(
      new MockAckListener());
  QuicReferenceCountedPointer<MockAckListener> ack_listener2(
      new MockAckListener());
  QuicReferenceCountedPointer<MockAckListener> ack_listener3(
      new MockAckListener());

  // Send [0, 42).
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);

  // Ack [15, 20), [5, 25), [10, 17), [0, 12) and [22, 42).
  EXPECT_CALL(*ack_listener2, OnPacketAcked(5, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(15, 5, false,
                                                  QuicTime::Delta::Zero()));

  EXPECT_CALL(*ack_listener1, OnPacketAcked(9, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(1, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(1, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(4, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(5, 20, false,
                                                  QuicTime::Delta::Zero()));

  // Duplicate ack.
  EXPECT_FALSE(headers_stream_->OnStreamFrameAcked(10, 7, false,
                                                   QuicTime::Delta::Zero()));

  EXPECT_CALL(*ack_listener1, OnPacketAcked(5, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(0, 12, false,
                                                  QuicTime::Delta::Zero()));

  EXPECT_CALL(*ack_listener3, OnPacketAcked(3, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(22, 20, false,
                                                  QuicTime::Delta::Zero()));
}

}  // namespace
}  // namespace test
}  // namespace quic
