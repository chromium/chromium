// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_UDP_SOCKET_TEST_UTIL_H_
#define SERVICES_NETWORK_TEST_UDP_SOCKET_TEST_UTIL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace test {

// Helper functions to invoke the corresponding mojo APIs and wait for
// completion.
class UDPSocketTestHelper {
 public:
  explicit UDPSocketTestHelper(mojo::Remote<mojom::UDPSocket>* socket);
  ~UDPSocketTestHelper();
  int ConnectSync(const net::IPEndPoint& remote_addr,
                  mojom::UDPSocketOptionsPtr options,
                  net::IPEndPoint* local_addr_out);
  int BindSync(const net::IPEndPoint& local_addr,
               mojom::UDPSocketOptionsPtr options,
               net::IPEndPoint* local_addr_out);
  int SendToSync(const net::IPEndPoint& remote_addr,
                 const std::vector<uint8_t>& input);
  int SendSync(const std::vector<uint8_t>& input);
  int SetBroadcastSync(bool broadcast);
  int SetSendBufferSizeSync(int send_buffer_size);
  int SetReceiveBufferSizeSync(int receive_buffer_size);
  int JoinGroupSync(const net::IPAddress& group_address);
  int LeaveGroupSync(const net::IPAddress& group_address);

 private:
  raw_ptr<mojo::Remote<mojom::UDPSocket>, DanglingUntriaged> socket_;
};

// An implementation of mojom::UDPSocketListener that records received results.
class UDPSocketListenerImpl : public mojom::UDPSocketListener {
 public:
  struct ReceivedResult {
    ReceivedResult(int net_error_arg,
                   const absl::optional<net::IPEndPoint>& src_addr_arg,
                   absl::optional<std::vector<uint8_t>> data_arg);
    ReceivedResult(const ReceivedResult& other);
    ~ReceivedResult();

    int net_error;
    absl::optional<net::IPEndPoint> src_addr;
    absl::optional<std::vector<uint8_t>> data;
  };

  UDPSocketListenerImpl();

  UDPSocketListenerImpl(const UDPSocketListenerImpl&) = delete;
  UDPSocketListenerImpl& operator=(const UDPSocketListenerImpl&) = delete;

  ~UDPSocketListenerImpl() override;

  const std::vector<ReceivedResult>& results() const { return results_; }

  void WaitForReceivedResults(size_t count);

 private:
  void OnReceived(int32_t result,
                  const absl::optional<net::IPEndPoint>& src_addr,
                  absl::optional<base::span<const uint8_t>> data) override;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<ReceivedResult> results_;
  size_t expected_receive_count_;
};

}  // namespace test

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_UDP_SOCKET_TEST_UTIL_H_
