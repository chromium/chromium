// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/server/http_server.h"
#include "net/socket/fuzzed_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace {

// Restrict the max size of the input. This prevents a timeout when the fuzzer
// finds the O(N^2) behavior of header parsing.
// TODO(https://crbug.com/370858119): Increase the limit if the O(N^2) behavior
// is fixed.
constexpr size_t kMaxInputSize = 32 * 1024;

class WaitTillHttpCloseDelegate : public net::HttpServer::Delegate {
 public:
  WaitTillHttpCloseDelegate(FuzzedDataProvider* data_provider,
                            base::OnceClosure done_closure)
      : data_provider_(data_provider),
        done_closure_(std::move(done_closure)),
        action_flags_(data_provider_->ConsumeIntegral<uint8_t>()) {}

  WaitTillHttpCloseDelegate(const WaitTillHttpCloseDelegate&) = delete;
  WaitTillHttpCloseDelegate& operator=(const WaitTillHttpCloseDelegate&) =
      delete;

  void set_server(net::HttpServer* server) { server_ = server; }

  void OnConnect(int connection_id) override {
    if (!(action_flags_ & ACCEPT_CONNECTION))
      server_->Close(connection_id);
  }

  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override {
    if (!(action_flags_ & ACCEPT_MESSAGE)) {
      server_->Close(connection_id);
      return;
    }

    if (action_flags_ & REPLY_TO_MESSAGE) {
      server_->Send200(connection_id,
                       data_provider_->ConsumeRandomLengthString(64),
                       "text/html", TRAFFIC_ANNOTATION_FOR_TESTS);
    }
  }

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override {
    if (action_flags_ & CLOSE_WEBSOCKET_RATHER_THAN_ACCEPT) {
      server_->Close(connection_id);
      return;
    }

    if (action_flags_ & ACCEPT_WEBSOCKET)
      server_->AcceptWebSocket(connection_id, info,
                               TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {
    if (!(action_flags_ & ACCEPT_MESSAGE)) {
      server_->Close(connection_id);
      return;
    }

    if (action_flags_ & REPLY_TO_MESSAGE) {
      server_->SendOverWebSocket(connection_id,
                                 data_provider_->ConsumeRandomLengthString(64),
                                 TRAFFIC_ANNOTATION_FOR_TESTS);
    }
  }

  void OnClose(int connection_id) override {
    // In general, OnClose can be called more than once, but FuzzedServerSocket
    // only makes one connection, and it is the only socket of interest here.
    std::move(done_closure_).Run();
  }

 private:
  enum {
    ACCEPT_CONNECTION = 1,
    ACCEPT_MESSAGE = 2,
    REPLY_TO_MESSAGE = 4,
    ACCEPT_WEBSOCKET = 8,
    CLOSE_WEBSOCKET_RATHER_THAN_ACCEPT = 16
  };

  raw_ptr<net::HttpServer> server_ = nullptr;
  const raw_ptr<FuzzedDataProvider> data_provider_;
  base::OnceClosure done_closure_;
  const uint8_t action_flags_;
};

}  // namespace

// Fuzzer for HttpServer
//
// |data| is used to create a FuzzedServerSocket.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > kMaxInputSize) {
    return 0;
  }

  // Including an observer; even though the recorded results aren't currently
  // used, it'll ensure the netlogging code is fuzzed as well.
  net::RecordingNetLogObserver net_log_observer;
  FuzzedDataProvider data_provider(data, size);

  std::unique_ptr<net::ServerSocket> server_socket(
      std::make_unique<net::FuzzedServerSocket>(&data_provider,
                                                net::NetLog::Get()));
  CHECK_EQ(net::OK,
           server_socket->ListenWithAddressAndPort("127.0.0.1", 80, 5));

  base::RunLoop run_loop;
  WaitTillHttpCloseDelegate delegate(&data_provider, run_loop.QuitClosure());
  net::HttpServer server(std::move(server_socket), &delegate);
  delegate.set_server(&server);
  run_loop.Run();
  return 0;
}
