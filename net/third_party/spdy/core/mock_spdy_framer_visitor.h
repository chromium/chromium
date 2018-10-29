// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_MOCK_SPDY_FRAMER_VISITOR_H_
#define NET_THIRD_PARTY_SPDY_CORE_MOCK_SPDY_FRAMER_VISITOR_H_

#include <cstdint>
#include <memory>

#include "net/third_party/spdy/core/http2_frame_decoder_adapter.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"
#include "net/third_party/spdy/platform/api/spdy_ptr_util.h"
#include "net/third_party/spdy/platform/api/spdy_string_piece.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace spdy {

namespace test {

class MockSpdyFramerVisitor : public SpdyFramerVisitorInterface {
 public:
  MockSpdyFramerVisitor();
  ~MockSpdyFramerVisitor() override;

  MOCK_METHOD1(OnError,
               void(http2::Http2DecoderAdapter::SpdyFramerError error));
  MOCK_METHOD3(OnDataFrameHeader,
               void(SpdyStreamId stream_id, size_t length, bool fin));
  MOCK_METHOD3(OnStreamFrameData,
               void(SpdyStreamId stream_id, const char* data, size_t len));
  MOCK_METHOD1(OnStreamEnd, void(SpdyStreamId stream_id));
  MOCK_METHOD2(OnStreamPadLength, void(SpdyStreamId stream_id, size_t value));
  MOCK_METHOD2(OnStreamPadding, void(SpdyStreamId stream_id, size_t len));
  MOCK_METHOD1(OnHeaderFrameStart,
               SpdyHeadersHandlerInterface*(SpdyStreamId stream_id));
  MOCK_METHOD1(OnHeaderFrameEnd, void(SpdyStreamId stream_id));
  MOCK_METHOD2(OnRstStream,
               void(SpdyStreamId stream_id, SpdyErrorCode error_code));
  MOCK_METHOD0(OnSettings, void());
  MOCK_METHOD2(OnSetting, void(SpdySettingsId id, uint32_t value));
  MOCK_METHOD2(OnPing, void(SpdyPingId unique_id, bool is_ack));
  MOCK_METHOD0(OnSettingsEnd, void());
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
  MOCK_METHOD3(OnPushPromise,
               void(SpdyStreamId stream_id,
                    SpdyStreamId promised_stream_id,
                    bool end));
  MOCK_METHOD2(OnContinuation, void(SpdyStreamId stream_id, bool end));
  MOCK_METHOD3(OnAltSvc,
               void(SpdyStreamId stream_id,
                    SpdyStringPiece origin,
                    const SpdyAltSvcWireFormat::AlternativeServiceVector&
                        altsvc_vector));
  MOCK_METHOD4(OnPriority,
               void(SpdyStreamId stream_id,
                    SpdyStreamId parent_stream_id,
                    int weight,
                    bool exclusive));
  MOCK_METHOD2(OnUnknownFrame,
               bool(SpdyStreamId stream_id, uint8_t frame_type));

  void DelegateHeaderHandling() {
    ON_CALL(*this, OnHeaderFrameStart(testing::_))
        .WillByDefault(testing::Invoke(
            this, &MockSpdyFramerVisitor::ReturnTestHeadersHandler));
    ON_CALL(*this, OnHeaderFrameEnd(testing::_))
        .WillByDefault(testing::Invoke(
            this, &MockSpdyFramerVisitor::ResetTestHeadersHandler));
  }

  SpdyHeadersHandlerInterface* ReturnTestHeadersHandler(
      SpdyStreamId /* stream_id */) {
    if (headers_handler_ == nullptr) {
      headers_handler_ = SpdyMakeUnique<TestHeadersHandler>();
    }
    return headers_handler_.get();
  }

  void ResetTestHeadersHandler(SpdyStreamId /* stream_id */) {
    headers_handler_.reset();
  }

  std::unique_ptr<SpdyHeadersHandlerInterface> headers_handler_;
};

}  // namespace test

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_MOCK_SPDY_FRAMER_VISITOR_H_
