// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/udp_socket_test_util.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace test {

UDPSocketTestHelper::UDPSocketTestHelper(mojo::Remote<mojom::UDPSocket>* socket)
    : socket_(socket) {}

UDPSocketTestHelper::~UDPSocketTestHelper() {}

int UDPSocketTestHelper::ConnectSync(const net::IPEndPoint& remote_addr,
                                     mojom::UDPSocketOptionsPtr options,
                                     net::IPEndPoint* local_addr_out) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->Connect(
      remote_addr, std::move(options),
      base::BindLambdaForTesting(
          [&](int result, const base::Optional<net::IPEndPoint>& local_addr) {
            net_error = result;
            if (local_addr) {
              *local_addr_out = local_addr.value();
            }
            run_loop.Quit();
          }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::BindSync(const net::IPEndPoint& local_addr,
                                  mojom::UDPSocketOptionsPtr options,
                                  net::IPEndPoint* local_addr_out) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->Bind(
      local_addr, std::move(options),
      base::BindLambdaForTesting(
          [&](int result, const base::Optional<net::IPEndPoint>& local_addr) {
            net_error = result;
            if (local_addr) {
              *local_addr_out = local_addr.value();
            }
            run_loop.Quit();
          }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SendToSync(const net::IPEndPoint& remote_addr,
                                    const std::vector<uint8_t>& input) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->SendTo(
      remote_addr, input,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      base::BindLambdaForTesting([&](int result) {
        net_error = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SendSync(const std::vector<uint8_t>& input) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->Send(
      input,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      base::BindLambdaForTesting([&](int result) {
        net_error = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SetBroadcastSync(bool broadcast) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->SetBroadcast(broadcast,
                               base::BindLambdaForTesting([&](int result) {
                                 net_error = result;
                                 run_loop.Quit();
                               }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SetSendBufferSizeSync(int send_buffer_size) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->SetSendBufferSize(send_buffer_size,
                                    base::BindLambdaForTesting([&](int result) {
                                      net_error = result;
                                      run_loop.Quit();
                                    }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SetReceiveBufferSizeSync(int receive_buffer_size) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->SetReceiveBufferSize(
      receive_buffer_size, base::BindLambdaForTesting([&](int result) {
        net_error = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::JoinGroupSync(const net::IPAddress& group_address) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->JoinGroup(group_address,
                            base::BindLambdaForTesting([&](int result) {
                              net_error = result;
                              run_loop.Quit();
                            }));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::LeaveGroupSync(const net::IPAddress& group_address) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket_->get()->LeaveGroup(group_address,
                             base::BindLambdaForTesting([&](int result) {
                               net_error = result;
                               run_loop.Quit();
                             }));
  run_loop.Run();
  return net_error;
}

UDPSocketListenerImpl::ReceivedResult::ReceivedResult(
    int net_error_arg,
    const base::Optional<net::IPEndPoint>& src_addr_arg,
    base::Optional<std::vector<uint8_t>> data_arg)
    : net_error(net_error_arg),
      src_addr(src_addr_arg),
      data(std::move(data_arg)) {}

UDPSocketListenerImpl::ReceivedResult::ReceivedResult(
    const ReceivedResult& other) = default;

UDPSocketListenerImpl::ReceivedResult::~ReceivedResult() {}

UDPSocketListenerImpl::UDPSocketListenerImpl()
    : run_loop_(std::make_unique<base::RunLoop>()),
      expected_receive_count_(0) {}

UDPSocketListenerImpl::~UDPSocketListenerImpl() {}

void UDPSocketListenerImpl::WaitForReceivedResults(size_t count) {
  DCHECK_LE(results_.size(), count);
  DCHECK_EQ(0u, expected_receive_count_);

  if (results_.size() == count)
    return;

  expected_receive_count_ = count;
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void UDPSocketListenerImpl::OnReceived(
    int32_t result,
    const base::Optional<net::IPEndPoint>& src_addr,
    base::Optional<base::span<const uint8_t>> data) {
  // OnReceive() API contracts specifies that this method will not be called
  // with a |result| that is > 0.
  DCHECK_GE(0, result);
  DCHECK(result < 0 || data);

  results_.emplace_back(result, src_addr,
                        data ? base::make_optional(std::vector<uint8_t>(
                                   data.value().begin(), data.value().end()))
                             : base::nullopt);
  if (results_.size() == expected_receive_count_) {
    expected_receive_count_ = 0;
    run_loop_->Quit();
  }
}

}  // namespace test

}  // namespace network
