// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP2_CONNECTION_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP2_CONNECTION_H_

#include <memory>
#include <queue>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/third_party/quiche/src/quiche/http2/adapter/http2_visitor_interface.h"
#include "net/third_party/quiche/src/quiche/http2/adapter/oghttp2_adapter.h"

namespace net::test_server {

using StreamId = http2::adapter::Http2StreamId;
template <class T>
using StreamMap = base::flat_map<StreamId, T>;

class EmbeddedTestServer;

// Outside of the text/binary (which is just a drop-in parser/decoder
// replacement) the main difference from Http1Connection is that multiple
// request/response "streams" can exist on the same connection, which means
// connections don't open on first request and don't close on first response
class Http2Connection : public HttpConnection,
                        public http2::adapter::Http2VisitorInterface {
 public:
  Http2Connection(std::unique_ptr<StreamSocket> socket,
                  EmbeddedTestServerConnectionListener* connection_listener,
                  EmbeddedTestServer* server_delegate);
  ~Http2Connection() override;
  Http2Connection(const HttpConnection&) = delete;
  Http2Connection& operator=(const Http2Connection&) = delete;

  // HttpConnection
  void OnSocketReady() override;
  StreamSocket* Socket() override;
  std::unique_ptr<StreamSocket> TakeSocket() override;
  base::WeakPtr<HttpConnection> GetWeakPtr() override;

  // http2::adapter::Http2VisitorInterface
  int64_t OnReadyToSend(std::string_view serialized) override;
  OnHeaderResult OnHeaderForStream(StreamId stream_id,
                                   std::string_view key,
                                   std::string_view value) override;
  bool OnEndHeadersForStream(StreamId stream_id) override;
  bool OnEndStream(StreamId stream_id) override;
  bool OnCloseStream(StreamId stream_id,
                     http2::adapter::Http2ErrorCode error_code) override;
  // Unused functions
  void OnConnectionError(ConnectionError /*error*/) override {}
  bool OnFrameHeader(StreamId /*stream_id*/,
                     size_t /*length*/,
                     uint8_t /*type*/,
                     uint8_t /*flags*/) override;
  void OnSettingsStart() override {}
  void OnSetting(http2::adapter::Http2Setting setting) override {}
  void OnSettingsEnd() override {}
  void OnSettingsAck() override {}
  bool OnBeginHeadersForStream(StreamId stream_id) override;
  bool OnBeginDataForStream(StreamId stream_id, size_t payload_length) override;
  bool OnDataForStream(StreamId stream_id, std::string_view data) override;
  bool OnDataPaddingLength(StreamId stream_id, size_t padding_length) override;
  void OnRstStream(StreamId stream_id,
                   http2::adapter::Http2ErrorCode error_code) override {}
  void OnPriorityForStream(StreamId stream_id,
                           StreamId parent_stream_id,
                           int weight,
                           bool exclusive) override {}
  void OnPing(http2::adapter::Http2PingId ping_id, bool is_ack) override {}
  void OnPushPromiseForStream(StreamId stream_id,
                              StreamId promised_stream_id) override {}
  bool OnGoAway(StreamId last_accepted_stream_id,
                http2::adapter::Http2ErrorCode error_code,
                std::string_view opaque_data) override;
  void OnWindowUpdate(StreamId stream_id, int window_increment) override {}
  int OnBeforeFrameSent(uint8_t frame_type,
                        StreamId stream_id,
                        size_t length,
                        uint8_t flags) override;
  int OnFrameSent(uint8_t frame_type,
                  StreamId stream_id,
                  size_t length,
                  uint8_t flags,
                  uint32_t error_code) override;
  bool OnInvalidFrame(StreamId stream_id, InvalidFrameError error) override;
  void OnBeginMetadataForStream(StreamId stream_id,
                                size_t payload_length) override {}
  bool OnMetadataForStream(StreamId stream_id,
                           std::string_view metadata) override;
  bool OnMetadataEndForStream(StreamId stream_id) override;
  void OnErrorDebug(std::string_view message) override {}

  http2::adapter::OgHttp2Adapter* adapter() { return adapter_.get(); }

 private:
  // Corresponds to one HTTP/2 stream in a connection
  class ResponseDelegate;
  class DataFrameSource;

  void ReadData();
  void OnDataRead(int rv);
  bool HandleData(int rv);
  void SendInternal();
  void OnSendInternalDone(int rv);

  void SendIfNotProcessing();

  StreamMap<std::unique_ptr<HttpRequest>> request_map_;
  StreamMap<std::unique_ptr<ResponseDelegate>> response_map_;
  StreamMap<HttpRequest::HeaderMap> header_map_;
  std::queue<StreamId> ready_streams_;
  std::unique_ptr<http2::adapter::OgHttp2Adapter> adapter_;
  std::unique_ptr<StreamSocket> socket_;
  const raw_ptr<EmbeddedTestServerConnectionListener> connection_listener_;
  const raw_ptr<EmbeddedTestServer> embedded_test_server_;
  scoped_refptr<IOBufferWithSize> read_buf_;
  // Frames can be submitted asynchronusly, so frames will be pulled one at a
  // time by the data frame through ReadyToSend. If the buffer is not null, it
  // is being processed and new frames should be blocked.
  scoped_refptr<DrainableIOBuffer> write_buf_{nullptr};
  // Streams from a DataFrameSource that were blocked.
  base::flat_set<StreamId> blocked_streams_;
  // Whether the connection is in the midst of processing requests, and will
  // send queued frames and data sources. Stops early on an I/O block or
  // depleted flow-control window.
  bool processing_responses_ = false;

  base::WeakPtrFactory<Http2Connection> weak_factory_{this};
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP2_CONNECTION_H_
