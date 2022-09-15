// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <vector>

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_udp_socket_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UDPSocketPrivate);

namespace {

const uint16_t kPortScanFrom = 1024;
const uint16_t kPortScanTo = 4096;

}  // namespace

TestUDPSocketPrivate::TestUDPSocketPrivate(
    TestingInstance* instance)
    : TestCase(instance) {
}

bool TestUDPSocketPrivate::Init() {
  bool tcp_socket_private_is_available = pp::TCPSocketPrivate::IsAvailable();
  if (!tcp_socket_private_is_available)
    instance_->AppendError("PPB_TCPSocket_Private interface not available");

  bool udp_socket_private_is_available = pp::UDPSocketPrivate::IsAvailable();
  if (!udp_socket_private_is_available)
    instance_->AppendError("PPB_UDPSocket_Private interface not available");

  bool net_address_private_is_available = pp::NetAddressPrivate::IsAvailable();
  if (!net_address_private_is_available)
    instance_->AppendError("PPB_NetAddress_Private interface not available");

  bool init_host_port = GetLocalHostPort(instance_->pp_instance(),
                                         &host_, &port_);
  if (!init_host_port)
    instance_->AppendError("Can't init host and port");

  return tcp_socket_private_is_available &&
      udp_socket_private_is_available &&
      net_address_private_is_available &&
      init_host_port &&
      CheckTestingInterface() &&
      EnsureRunningOverHTTP();
}

void TestUDPSocketPrivate::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestUDPSocketPrivate, Connect, filter);
  RUN_CALLBACK_TEST(TestUDPSocketPrivate, ConnectFailure, filter);
  RUN_CALLBACK_TEST(TestUDPSocketPrivate, Broadcast, filter);
  RUN_CALLBACK_TEST(TestUDPSocketPrivate, SetSocketFeatureErrors, filter);
}

std::string TestUDPSocketPrivate::GetLocalAddress(
    PP_NetAddress_Private* address) {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      socket.Connect(host_.c_str(), port_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_TRUE(socket.GetLocalAddress(address));
  socket.Disconnect();
  PASS();
}

std::string TestUDPSocketPrivate::SetBroadcastOptions(
    pp::UDPSocketPrivate* socket) {
  int32_t rv = socket->SetSocketFeature(
      PP_UDPSOCKETFEATURE_PRIVATE_ADDRESS_REUSE, pp::Var(true));
  if (rv != PP_OK)
    return ReportError("PPB_UDPSocket_Private::SetSocketFeature", rv);

  rv = socket->SetSocketFeature(PP_UDPSOCKETFEATURE_PRIVATE_BROADCAST,
                                pp::Var(true));
  if (rv != PP_OK)
    return ReportError("PPB_UDPSocket_Private::SetSocketFeature", rv);

  PASS();
}

std::string TestUDPSocketPrivate::BindUDPSocket(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private* address) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket->Bind(address, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  PASS();
}

std::string TestUDPSocketPrivate::LookupPortAndBindUDPSocket(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private *address) {
  PP_NetAddress_Private base_address;
  ASSERT_SUBTEST_SUCCESS(GetLocalAddress(&base_address));

  bool is_free_port_found = false;
  for (uint16_t port = kPortScanFrom; port < kPortScanTo; ++port) {
    if (!pp::NetAddressPrivate::ReplacePort(base_address, port, address))
      return "PPB_NetAddress_Private::ReplacePort: Failed";
    if (BindUDPSocket(socket, address).empty()) {
      is_free_port_found = true;
      break;
    }
  }
  if (!is_free_port_found)
    return "Can't find available port";
  if (!socket->GetBoundAddress(address))
    return "PPB_UDPSocket_Private::GetBoundAddress: Failed";
  PASS();
}

std::string TestUDPSocketPrivate::BindUDPSocketFailure(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private *address) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket->Bind(address, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_NE(PP_OK, callback.result());
  ASSERT_FALSE(socket->GetBoundAddress(address));
  PASS();
}

std::string TestUDPSocketPrivate::ReadSocket(pp::UDPSocketPrivate* socket,
                                             PP_NetAddress_Private* address,
                                             size_t size,
                                             std::string* message) {
  std::vector<char> buffer(size);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      socket->RecvFrom(&buffer[0], static_cast<int32_t>(size),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_FALSE(callback.result() < 0);
  ASSERT_EQ(size, static_cast<size_t>(callback.result()));
  message->assign(buffer.begin(), buffer.end());
  PASS();
}

std::string TestUDPSocketPrivate::PassMessage(pp::UDPSocketPrivate* target,
                                              pp::UDPSocketPrivate* source,
                                              PP_NetAddress_Private* address,
                                              const std::string& message) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  int32_t rv = source->SendTo(message.c_str(),
                              static_cast<int32_t>(message.size()), address,
                              callback.GetCallback());
  std::string str;
  ASSERT_SUBTEST_SUCCESS(ReadSocket(target, address, message.size(), &str));

  callback.WaitForResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_FALSE(callback.result() < 0);
  ASSERT_EQ(message.size(), static_cast<size_t>(callback.result()));
  ASSERT_EQ(message, str);
  PASS();
}

std::string TestUDPSocketPrivate::TestConnect() {
  pp::UDPSocketPrivate server_socket(instance_), client_socket(instance_);
  PP_NetAddress_Private server_address, client_address;

  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&server_socket,
                                                    &server_address));
  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&client_socket,
                                                    &client_address));
  const std::string message = "Simple message that will be sent via UDP";
  ASSERT_SUBTEST_SUCCESS(PassMessage(&server_socket, &client_socket,
                                     &server_address,
                                     message));
  PP_NetAddress_Private recv_from_address;
  ASSERT_TRUE(server_socket.GetRecvFromAddress(&recv_from_address));
  ASSERT_TRUE(pp::NetAddressPrivate::AreEqual(recv_from_address,
                                              client_address));

  server_socket.Close();
  client_socket.Close();

  if (server_socket.GetBoundAddress(&server_address))
    return "PPB_UDPSocket_Private::GetBoundAddress: expected Failure";
  PASS();
}

std::string TestUDPSocketPrivate::TestConnectFailure() {
  pp::UDPSocketPrivate socket(instance_);
  PP_NetAddress_Private invalid_address = {};

  std::string error_message = BindUDPSocketFailure(&socket, &invalid_address);
  if (!error_message.empty())
    return error_message;

  PASS();
}

std::string TestUDPSocketPrivate::TestBroadcast() {
  const uint8_t broadcast_ip[4] = { 0xff, 0xff, 0xff, 0xff };

  pp::UDPSocketPrivate server1(instance_), server2(instance_);

  ASSERT_SUBTEST_SUCCESS(SetBroadcastOptions(&server1));
  ASSERT_SUBTEST_SUCCESS(SetBroadcastOptions(&server2));
  PP_NetAddress_Private server_address;
  ASSERT_TRUE(pp::NetAddressPrivate::GetAnyAddress(false, &server_address));
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&server1, &server_address));
  // Fill port field of |server_address|.
  ASSERT_TRUE(server1.GetBoundAddress(&server_address));
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&server2, &server_address));

  const uint16_t port = pp::NetAddressPrivate::GetPort(server_address);
  PP_NetAddress_Private broadcast_address;
  ASSERT_TRUE(pp::NetAddressPrivate::CreateFromIPv4Address(
      broadcast_ip, port, &broadcast_address));

  std::string message;
  const std::string first_message = "first message";
  const std::string second_message = "second_message";

  ASSERT_SUBTEST_SUCCESS(PassMessage(&server1, &server2,
                                     &broadcast_address,
                                     first_message));
  // |first_message| also arrived to |server2|.
  ASSERT_SUBTEST_SUCCESS(ReadSocket(&server2, &broadcast_address,
                                    first_message.size(), &message));
  ASSERT_EQ(first_message, message);

  ASSERT_SUBTEST_SUCCESS(PassMessage(&server2, &server1,
                                     &broadcast_address,
                                     second_message));
  // |second_message| also arrived to |server1|.
  ASSERT_SUBTEST_SUCCESS(ReadSocket(&server1, &broadcast_address,
                                    second_message.size(), &message));
  ASSERT_EQ(second_message, message);

  server1.Close();
  server2.Close();
  PASS();
}

std::string TestUDPSocketPrivate::TestSetSocketFeatureErrors() {
  pp::UDPSocketPrivate socket(instance_);
  // Try to pass incorrect feature name.
  int32_t rv = socket.SetSocketFeature(PP_UDPSOCKETFEATURE_PRIVATE_COUNT,
                                       pp::Var(true));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, rv);

  // Try to pass incorrect feature value's type.
  rv = socket.SetSocketFeature(PP_UDPSOCKETFEATURE_PRIVATE_ADDRESS_REUSE,
                               pp::Var(1));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, rv);
  PASS();
}
