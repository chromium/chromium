// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "net/base/url_util.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"

#include "net/tools/quic/quic_http_proxy_backend.h"
#include "net/tools/quic/quic_http_proxy_backend_stream.h"

namespace net {
namespace test {

class TestQuicServerStream
    : public quic::QuicSimpleServerBackend::RequestHandler {
 public:
  TestQuicServerStream() : did_complete_(false) {}

  ~TestQuicServerStream() override {}

  quic::QuicConnectionId connection_id() const override {
    return quic::test::TestConnectionId(123);
  }
  quic::QuicStreamId stream_id() const override { return 5; }
  std::string peer_host() const override { return "127.0.0.1"; }

  void OnResponseBackendComplete(
      const quic::QuicBackendResponse* response,
      std::list<quic::QuicBackendResponse::ServerPushInfo> resources) override {
    EXPECT_FALSE(did_complete_);
    did_complete_ = true;
    task_runner_->PostTask(FROM_HERE, run_loop_.QuitClosure());
  }

  base::RunLoop* run_loop() { return &run_loop_; }

 private:
  bool did_complete_;
  base::test::TaskEnvironment task_environment;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::ThreadTaskRunnerHandle::Get();
  base::RunLoop run_loop_;
};

class QuicHttpProxyBackendTest : public QuicTest {
 public:
  QuicHttpProxyBackendTest() {
    proxy_stream_map_ = http_proxy_.proxy_backend_streams_map();
  }

  ~QuicHttpProxyBackendTest() override {
    EXPECT_EQ(true, proxy_stream_map_->empty());
  }

  void SendRequestOverBackend(TestQuicServerStream* quic_stream) {
    quic_proxy_backend_url_ = "http://www.google.com:80";
    http_proxy_.InitializeBackend(quic_proxy_backend_url_);

    spdy::SpdyHeaderBlock request_headers;
    request_headers[":authority"] = "www.example.org";
    request_headers[":method"] = "GET";
    std::string body = "Test Body";
    http_proxy_.FetchResponseFromBackend(request_headers, body, quic_stream);
    quic_stream->run_loop()->Run();
  }

 protected:
  std::string quic_proxy_backend_url_;
  QuicHttpProxyBackend http_proxy_;
  const QuicHttpProxyBackend::ProxyBackendStreamMap* proxy_stream_map_;
};

TEST_F(QuicHttpProxyBackendTest, InitializeQuicHttpProxyBackend) {
  // Test incorrect URLs
  quic_proxy_backend_url_ = "http://www.google.com:80--";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_EQ(false, http_proxy_.IsBackendInitialized());
  EXPECT_EQ(nullptr, http_proxy_.GetProxyTaskRunner());

  quic_proxy_backend_url_ = "http://192.168.239.257:80";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_EQ(false, http_proxy_.IsBackendInitialized());
  EXPECT_EQ(nullptr, http_proxy_.GetProxyTaskRunner());

  quic_proxy_backend_url_ = "http://2555.168.239:80";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_EQ(false, http_proxy_.IsBackendInitialized());
  EXPECT_EQ(nullptr, http_proxy_.GetProxyTaskRunner());

  quic_proxy_backend_url_ = "http://192.168.239.237:65537";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_EQ(false, http_proxy_.IsBackendInitialized());
  EXPECT_EQ(nullptr, http_proxy_.GetProxyTaskRunner());

  quic_proxy_backend_url_ = "ftp://www.google.com:80";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_EQ(false, http_proxy_.IsBackendInitialized());
  EXPECT_EQ(nullptr, http_proxy_.GetProxyTaskRunner());

  // Test initialization with correct URL
  quic_proxy_backend_url_ = "http://www.google.com:80";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_NE(nullptr, http_proxy_.GetProxyTaskRunner());
  EXPECT_EQ("http://www.google.com/", http_proxy_.backend_url());
  EXPECT_EQ(true, http_proxy_.IsBackendInitialized());
}

TEST_F(QuicHttpProxyBackendTest, CheckProxyStreamManager) {
  TestQuicServerStream quic_stream;
  SendRequestOverBackend(&quic_stream);
  auto it_find_success = proxy_stream_map_->find(&quic_stream);
  EXPECT_NE(it_find_success, proxy_stream_map_->end());

  http_proxy_.CloseBackendResponseStream(&quic_stream);

  /*EXPECT_EQ(true, proxy_stream_map_->empty());
  QuicHttpProxyBackend::ProxyBackendStreamMap::const_iterator it_find_fail =
      proxy_stream_map_->find(&quic_stream);
  EXPECT_EQ(it_find_fail, proxy_stream_map_->end());*/
}

TEST_F(QuicHttpProxyBackendTest, CheckIsOnBackendThread) {
  quic_proxy_backend_url_ = "http://www.google.com:80";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_EQ(false, http_proxy_.GetProxyTaskRunner()->BelongsToCurrentThread());
}

TEST_F(QuicHttpProxyBackendTest, CheckGetBackendTaskRunner) {
  EXPECT_EQ(nullptr, http_proxy_.GetProxyTaskRunner());
  quic_proxy_backend_url_ = "http://www.google.com:80";
  http_proxy_.InitializeBackend(quic_proxy_backend_url_);
  EXPECT_NE(nullptr, http_proxy_.GetProxyTaskRunner());
}

}  // namespace test
}  // namespace net
