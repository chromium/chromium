// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_udp_socket.h"

#include <vector>

#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/cpp/udp_socket.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UDPSocket);

namespace {

const uint16_t kPortScanFrom = 1024;
const uint16_t kPortScanTo = 4096;

pp::NetAddress ReplacePort(const pp::InstanceHandle& instance,
                           const pp::NetAddress& addr,
                           uint16_t port) {
  switch (addr.GetFamily()) {
    case PP_NETADDRESS_FAMILY_IPV4: {
      PP_NetAddress_IPv4 ipv4_addr;
      if (!addr.DescribeAsIPv4Address(&ipv4_addr))
        break;
      ipv4_addr.port = ConvertToNetEndian16(port);
      return pp::NetAddress(instance, ipv4_addr);
    }
    case PP_NETADDRESS_FAMILY_IPV6: {
      PP_NetAddress_IPv6 ipv6_addr;
      if (!addr.DescribeAsIPv6Address(&ipv6_addr))
        break;
      ipv6_addr.port = ConvertToNetEndian16(port);
      return pp::NetAddress(instance, ipv6_addr);
    }
    default: {
      PP_NOTREACHED();
    }
  }
  return pp::NetAddress();
}

}  // namespace

TestUDPSocket::TestUDPSocket(TestingInstance* instance)
    : TestCase(instance),
      socket_interface_1_0_(NULL),
      socket_interface_1_1_(NULL) {
}

bool TestUDPSocket::Init() {
  bool tcp_socket_is_available = pp::TCPSocket::IsAvailable();
  if (!tcp_socket_is_available)
    instance_->AppendError("PPB_TCPSocket interface not available");

  bool udp_socket_is_available = pp::UDPSocket::IsAvailable();
  if (!udp_socket_is_available)
    instance_->AppendError("PPB_UDPSocket interface not available");

  bool net_address_is_available = pp::NetAddress::IsAvailable();
  if (!net_address_is_available)
    instance_->AppendError("PPB_NetAddress interface not available");

  std::string host;
  uint16_t port = 0;
  bool init_address =
      GetLocalHostPort(instance_->pp_instance(), &host, &port) &&
      ResolveHost(instance_->pp_instance(), host, port, &address_);
  if (!init_address)
    instance_->AppendError("Can't init address");

  socket_interface_1_0_ =
      static_cast<const PPB_UDPSocket_1_0*>(
          pp::Module::Get()->GetBrowserInterface(PPB_UDPSOCKET_INTERFACE_1_0));
  if (!socket_interface_1_0_)
    instance_->AppendError("PPB_UDPSocket_1_0 interface not available");

  socket_interface_1_1_ =
      static_cast<const PPB_UDPSocket_1_1*>(
          pp::Module::Get()->GetBrowserInterface(PPB_UDPSOCKET_INTERFACE_1_1));
  if (!socket_interface_1_1_)
    instance_->AppendError("PPB_UDPSocket_1_1 interface not available");

  return tcp_socket_is_available &&
      udp_socket_is_available &&
      net_address_is_available &&
      init_address &&
      CheckTestingInterface() &&
      EnsureRunningOverHTTP() &&
      socket_interface_1_0_ != NULL &&
      socket_interface_1_1_ != NULL;
}

void TestUDPSocket::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestUDPSocket, ReadWrite, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, Broadcast, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, SetOption_1_0, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, SetOption_1_1, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, SetOption, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, ParallelSend, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, Multicast, filter);

  // Failure tests. Generally can only be run individually, since they require
  // specific socket failures to be injected into the UDP code.
  RUN_CALLBACK_TEST(TestUDPSocket, BindFails, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, SetBroadcastFails, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, SendToFails, filter);
  RUN_CALLBACK_TEST(TestUDPSocket, ReadFails, filter);
}

std::string TestUDPSocket::GetLocalAddress(pp::NetAddress* address) {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket.Connect(address_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  *address = socket.GetLocalAddress();
  ASSERT_NE(0, address->pp_resource());
  socket.Close();
  PASS();
}

std::string TestUDPSocket::SetBroadcastOptions(pp::UDPSocket* socket) {
  TestCompletionCallback callback_1(instance_->pp_instance(), callback_type());
  callback_1.WaitForResult(socket->SetOption(
      PP_UDPSOCKET_OPTION_ADDRESS_REUSE, pp::Var(true),
      callback_1.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_1);
  ASSERT_EQ(PP_OK, callback_1.result());

  TestCompletionCallback callback_2(instance_->pp_instance(), callback_type());
  callback_2.WaitForResult(socket->SetOption(
      PP_UDPSOCKET_OPTION_BROADCAST, pp::Var(true), callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  ASSERT_EQ(PP_OK, callback_2.result());

  PASS();
}

std::string TestUDPSocket::BindUDPSocket(pp::UDPSocket* socket,
                                         const pp::NetAddress& address) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket->Bind(address, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  PASS();
}

std::string TestUDPSocket::LookupPortAndBindUDPSocket(
    pp::UDPSocket* socket,
    pp::NetAddress* address) {
  pp::NetAddress base_address;
  ASSERT_SUBTEST_SUCCESS(GetLocalAddress(&base_address));

  bool is_free_port_found = false;
  std::string ret;
  for (uint16_t port = kPortScanFrom; port < kPortScanTo; ++port) {
    pp::NetAddress new_address = ReplacePort(instance_, base_address, port);
    ASSERT_NE(0, new_address.pp_resource());
    ret = BindUDPSocket(socket, new_address);
    if (ret.empty()) {
      is_free_port_found = true;
      break;
    }
  }
  if (!is_free_port_found)
    return "Can't find available port (" + ret + ")";

  *address = socket->GetBoundAddress();
  ASSERT_NE(0, address->pp_resource());

  PASS();
}

std::string TestUDPSocket::ReadSocket(pp::UDPSocket* socket,
                                      pp::NetAddress* address,
                                      size_t size,
                                      std::string* message) {
  std::vector<char> buffer(size);
  TestCompletionCallbackWithOutput<pp::NetAddress> callback(
      instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      socket->RecvFrom(&buffer[0], static_cast<int32_t>(size),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_FALSE(callback.result() < 0);
  ASSERT_EQ(size, static_cast<size_t>(callback.result()));
  *address = callback.output();
  message->assign(buffer.begin(), buffer.end());
  PASS();
}

std::string TestUDPSocket::PassMessage(pp::UDPSocket* target,
                                       pp::UDPSocket* source,
                                       const pp::NetAddress& target_address,
                                       const std::string& message,
                                       pp::NetAddress* recvfrom_address) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  int32_t rv = source->SendTo(message.c_str(),
                              static_cast<int32_t>(message.size()),
                              target_address,
                              callback.GetCallback());
  std::string str;
  ASSERT_SUBTEST_SUCCESS(ReadSocket(target, recvfrom_address, message.size(),
                                    &str));

  callback.WaitForResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_FALSE(callback.result() < 0);
  ASSERT_EQ(message.size(), static_cast<size_t>(callback.result()));
  ASSERT_EQ(message, str);
  PASS();
}

std::string TestUDPSocket::SetMulticastOptions(pp::UDPSocket* socket) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket->SetOption(
      PP_UDPSOCKET_OPTION_MULTICAST_LOOP, pp::Var(true),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(socket->SetOption(
      PP_UDPSOCKET_OPTION_MULTICAST_TTL, pp::Var(1), callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  PASS();
}

std::string TestUDPSocket::TestReadWrite() {
  pp::UDPSocket server_socket(instance_), client_socket(instance_);
  pp::NetAddress server_address, client_address;

  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&server_socket,
                                                    &server_address));
  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&client_socket,
                                                    &client_address));
  const std::string message = "Simple message that will be sent via UDP";
  pp::NetAddress recvfrom_address;
  ASSERT_SUBTEST_SUCCESS(PassMessage(&server_socket, &client_socket,
                                     server_address, message,
                                     &recvfrom_address));
  ASSERT_TRUE(EqualNetAddress(recvfrom_address, client_address));

  server_socket.Close();
  client_socket.Close();

  if (server_socket.GetBoundAddress().pp_resource() != 0)
    return "PPB_UDPSocket::GetBoundAddress: expected failure";

  PASS();
}

std::string TestUDPSocket::TestBroadcast() {
  pp::UDPSocket server1(instance_), server2(instance_);

  ASSERT_SUBTEST_SUCCESS(SetBroadcastOptions(&server1));
  ASSERT_SUBTEST_SUCCESS(SetBroadcastOptions(&server2));

  PP_NetAddress_IPv4 any_ipv4_address = { 0, { 0, 0, 0, 0 } };
  pp::NetAddress any_address(instance_, any_ipv4_address);
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&server1, any_address));
  // Fill port field of |server_address|.
  pp::NetAddress server_address = server1.GetBoundAddress();
  ASSERT_NE(0, server_address.pp_resource());
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&server2, server_address));

  PP_NetAddress_IPv4 server_ipv4_address;
  ASSERT_TRUE(server_address.DescribeAsIPv4Address(&server_ipv4_address));

  PP_NetAddress_IPv4 broadcast_ipv4_address = {
    server_ipv4_address.port, { 0xff, 0xff, 0xff, 0xff }
  };
  pp::NetAddress broadcast_address(instance_, broadcast_ipv4_address);

  std::string message;
  const std::string first_message = "first message";
  const std::string second_message = "second_message";

  pp::NetAddress recvfrom_address;
  ASSERT_SUBTEST_SUCCESS(PassMessage(&server1, &server2, broadcast_address,
                                     first_message, &recvfrom_address));
  // |first_message| was also received by |server2|.
  ASSERT_SUBTEST_SUCCESS(ReadSocket(&server2, &recvfrom_address,
                                    first_message.size(), &message));
  ASSERT_EQ(first_message, message);

  ASSERT_SUBTEST_SUCCESS(PassMessage(&server2, &server1, broadcast_address,
                                     second_message, &recvfrom_address));
  // |second_message| was also received by |server1|.
  ASSERT_SUBTEST_SUCCESS(ReadSocket(&server1, &recvfrom_address,
                                    second_message.size(), &message));
  ASSERT_EQ(second_message, message);

  server1.Close();
  server2.Close();
  PASS();
}

int32_t TestUDPSocket::SetOptionValue(UDPSocketSetOption func,
                                      PP_Resource socket,
                                      PP_UDPSocket_Option option,
                                      const PP_Var& value) {
  PP_TimeTicks start_time(NowInTimeTicks());
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(func(socket, option, value,
                        cb.GetCallback().pp_completion_callback()));

  // Expanded from CHECK_CALLBACK_BEHAVIOR macro.
  if (cb.failed()) {
    std::string msg = MakeFailureMessage(__FILE__, __LINE__,
                                         cb.errors().c_str());

    instance_->LogTest("SetOptionValue", msg, start_time);
    return PP_ERROR_FAILED;
  }
  return cb.result();
}

std::string TestUDPSocket::TestSetOption_1_0() {
  PP_Resource socket = socket_interface_1_0_->Create(instance_->pp_instance());
  ASSERT_NE(0, socket);

  // Multicast options are not supported in interface 1.0.
  ASSERT_EQ(PP_ERROR_BADARGUMENT,
            SetOptionValue(socket_interface_1_0_->SetOption,
                           socket,
                           PP_UDPSOCKET_OPTION_MULTICAST_LOOP,
                           PP_MakeBool(PP_TRUE)));

  ASSERT_EQ(PP_ERROR_BADARGUMENT,
            SetOptionValue(socket_interface_1_0_->SetOption,
                           socket,
                           PP_UDPSOCKET_OPTION_MULTICAST_TTL,
                           PP_MakeInt32(1)));

  socket_interface_1_0_->Close(socket);
  pp::Module::Get()->core()->ReleaseResource(socket);

  PASS();
}

std::string TestUDPSocket::TestSetOption_1_1() {
  PP_Resource socket = socket_interface_1_1_->Create(instance_->pp_instance());
  ASSERT_NE(0, socket);

  // Multicast options are not supported in interface 1.1.
  ASSERT_EQ(PP_ERROR_BADARGUMENT,
            SetOptionValue(socket_interface_1_1_->SetOption,
                           socket,
                           PP_UDPSOCKET_OPTION_MULTICAST_LOOP,
                           PP_MakeBool(PP_TRUE)));

  ASSERT_EQ(PP_ERROR_BADARGUMENT,
            SetOptionValue(socket_interface_1_1_->SetOption,
                           socket,
                           PP_UDPSOCKET_OPTION_MULTICAST_TTL,
                           PP_MakeInt32(1)));

  socket_interface_1_1_->Close(socket);
  pp::Module::Get()->core()->ReleaseResource(socket);

  PASS();
}

std::string TestUDPSocket::TestSetOption() {
  pp::UDPSocket socket(instance_);

  ASSERT_SUBTEST_SUCCESS(SetBroadcastOptions(&socket));
  ASSERT_SUBTEST_SUCCESS(SetMulticastOptions(&socket));

  // Try to pass incorrect option value's type.
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_ADDRESS_REUSE, pp::Var(1), callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());

  // Invalid multicast TTL values (less than 0 and greater than 255).
  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_MULTICAST_TTL, pp::Var(-1), callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());

  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_MULTICAST_TTL, pp::Var(256), callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());

  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_BROADCAST, pp::Var(false), callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE, pp::Var(4096),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE, pp::Var(512),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::NetAddress address;
  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&socket, &address));

  // ADDRESS_REUSE won't take effect after the socket is bound.
  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_ADDRESS_REUSE, pp::Var(true),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  // BROADCAST, SEND_BUFFER_SIZE and RECV_BUFFER_SIZE can be set after the
  // socket is bound.
  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_BROADCAST, pp::Var(true), callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE, pp::Var(2048),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE, pp::Var(1024),
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  PASS();
}

std::string TestUDPSocket::TestParallelSend() {
  // This test only makes sense when callbacks are optional.
  if (callback_type() != PP_OPTIONAL)
    PASS();

  pp::UDPSocket server_socket(instance_), client_socket(instance_);
  pp::NetAddress server_address, client_address;

  ASSERT_SUBTEST_SUCCESS(
      LookupPortAndBindUDPSocket(&server_socket, &server_address));
  ASSERT_SUBTEST_SUCCESS(
      LookupPortAndBindUDPSocket(&client_socket, &client_address));
  const std::string message = "Simple message that will be sent via UDP";
  pp::NetAddress recvfrom_address;

  const size_t kParallelSends = 10;
  std::vector<TestCompletionCallback*> sendto_callbacks(kParallelSends);
  std::vector<int32_t> sendto_results(kParallelSends);
  size_t pending = 0;
  for (size_t i = 0; i < kParallelSends; i++) {
    sendto_callbacks[i] =
        new TestCompletionCallback(instance_->pp_instance(), callback_type());
    sendto_results[i] =
        client_socket.SendTo(message.c_str(),
                             static_cast<int32_t>(message.size()),
                             server_address,
                             sendto_callbacks[i]->GetCallback());

    if (sendto_results[i] == PP_ERROR_INPROGRESS) {
      // Run a pending send to completion to free a slot for the current send.
      ASSERT_GT(i, pending);
      sendto_callbacks[pending]->WaitForResult(sendto_results[pending]);
      CHECK_CALLBACK_BEHAVIOR(*sendto_callbacks[pending]);
      ASSERT_EQ(message.size(),
                static_cast<size_t>(sendto_callbacks[pending]->result()));
      pending++;
      // Try to send the message again.
      sendto_results[i] =
          client_socket.SendTo(message.c_str(),
                               static_cast<int32_t>(message.size()),
                               server_address,
                               sendto_callbacks[i]->GetCallback());
      ASSERT_NE(PP_ERROR_INPROGRESS, sendto_results[i]);
    }
  }

  // Finish all pending sends.
  for (size_t i = pending; i < kParallelSends; i++) {
    sendto_callbacks[i]->WaitForResult(sendto_results[i]);
    CHECK_CALLBACK_BEHAVIOR(*sendto_callbacks[i]);
    ASSERT_EQ(message.size(),
              static_cast<size_t>(sendto_callbacks[i]->result()));
  }

  for (size_t i = 0; i < kParallelSends; ++i)
    delete sendto_callbacks[i];

  for (size_t i = 0; i < kParallelSends; i++) {
    std::string str;
    ASSERT_SUBTEST_SUCCESS(
        ReadSocket(&server_socket, &recvfrom_address, message.size(), &str));
    ASSERT_EQ(message, str);
  }

  server_socket.Close();
  client_socket.Close();

  PASS();
}

std::string TestUDPSocket::TestMulticast() {
  pp::UDPSocket server1(instance_), server2(instance_);

  ASSERT_SUBTEST_SUCCESS(SetMulticastOptions(&server1));
  ASSERT_SUBTEST_SUCCESS(SetMulticastOptions(&server2));

  server1.Close();
  server2.Close();

  PASS();
}

std::string TestUDPSocket::TestBindFails() {
  pp::UDPSocket socket(instance_);

  PP_NetAddress_IPv4 any_ipv4_address = {0, {0, 0, 0, 0}};
  pp::NetAddress any_address(instance_, any_ipv4_address);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket.Bind(any_address, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  PASS();
}

std::string TestUDPSocket::TestSetBroadcastFails() {
  pp::UDPSocket socket(instance_);
  PP_NetAddress_IPv4 any_ipv4_address = {0, {0, 0, 0, 0}};
  pp::NetAddress any_address(instance_, any_ipv4_address);
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&socket, any_address));

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_BROADCAST, pp::Var(true), callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  // Setting broadcast again should also fail.
  TestCompletionCallback callback_2(instance_->pp_instance(), callback_type());
  callback_2.WaitForResult(socket.SetOption(
      PP_UDPSOCKET_OPTION_BROADCAST, pp::Var(true), callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  ASSERT_EQ(PP_ERROR_FAILED, callback_2.result());
  PASS();
}

std::string TestUDPSocket::TestSendToFails() {
  pp::UDPSocket socket(instance_);
  PP_NetAddress_IPv4 any_ipv4_address = {0, {0, 0, 0, 0}};
  pp::NetAddress any_address(instance_, any_ipv4_address);
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&socket, any_address));

  std::vector<char> buffer(1);
  buffer[0] = 1;
  PP_NetAddress_IPv4 target_ipv4_address = {1024, {127, 0, 0, 1}};
  pp::NetAddress target_address(instance_, target_ipv4_address);
  // All writes should fail.
  for (int i = 0; i < 10; ++i) {
    TestCompletionCallbackWithOutput<pp::NetAddress> callback(
        instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        socket.SendTo(buffer.data(), static_cast<int32_t>(buffer.size()),
                      target_address, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  }
  PASS();
}

std::string TestUDPSocket::TestReadFails() {
  pp::UDPSocket socket(instance_);
  PP_NetAddress_IPv4 any_ipv4_address = {0, {0, 0, 0, 0}};
  pp::NetAddress any_address(instance_, any_ipv4_address);
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&socket, any_address));

  std::vector<char> buffer(1);
  // All reads should fail. Larger number of reads increases the chance that at
  // least one read will be synchronous.
  for (int i = 0; i < 200; ++i) {
    TestCompletionCallbackWithOutput<pp::NetAddress> callback(
        instance_->pp_instance(), callback_type());
    callback.WaitForResult(socket.RecvFrom(&buffer[0],
                                           static_cast<int32_t>(buffer.size()),
                                           callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  }
  PASS();
}
