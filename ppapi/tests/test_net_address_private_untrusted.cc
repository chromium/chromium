// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_net_address_private_untrusted.h"

#include <limits>
#include <sstream>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(NetAddressPrivateUntrusted);

using pp::NetAddressPrivate;
using pp::TCPSocketPrivate;

TestNetAddressPrivateUntrusted::TestNetAddressPrivateUntrusted(
    TestingInstance* instance) : TestCase(instance), port_(0) {
}

bool TestNetAddressPrivateUntrusted::Init() {
  bool net_address_private_is_available = NetAddressPrivate::IsAvailable();
  if (!net_address_private_is_available)
    instance_->AppendError("PPB_NetAddress_Private interface not available");

  bool tcp_socket_private_is_available = TCPSocketPrivate::IsAvailable();
  if (!tcp_socket_private_is_available)
    instance_->AppendError("PPB_TCPSocket_Private interface not available");

  bool init_host_port =
      GetLocalHostPort(instance_->pp_instance(), &host_, &port_);
  if (!init_host_port)
    instance_->AppendError("Can't init host and port");

  return net_address_private_is_available &&
      tcp_socket_private_is_available &&
      init_host_port &&
      CheckTestingInterface();
}

void TestNetAddressPrivateUntrusted::RunTests(const std::string& filter) {
  RUN_TEST(AreEqual, filter);
  RUN_TEST(AreHostsEqual, filter);
  RUN_TEST(Describe, filter);
  RUN_TEST(ReplacePort, filter);
  RUN_TEST(GetAnyAddress, filter);
  RUN_TEST(GetFamily, filter);
  RUN_TEST(GetPort, filter);
  RUN_TEST(GetAddress, filter);
}

int32_t TestNetAddressPrivateUntrusted::Connect(TCPSocketPrivate* socket,
                                                const std::string& host,
                                                uint16_t port) {
  TestCompletionCallback callback(instance_->pp_instance(), false);

  callback.WaitForResult(
      socket->Connect(host.c_str(), port, callback.GetCallback()));
  return callback.result();
}

std::string TestNetAddressPrivateUntrusted::TestAreEqual() {
  pp::TCPSocketPrivate socket(instance_);
  int32_t rv = Connect(&socket, host_, port_);
  if (rv != PP_OK)
    return ReportError("pp::TCPSocketPrivate::Connect", rv);

  PP_NetAddress_Private local_address, remote_address;
  ASSERT_TRUE(socket.GetLocalAddress(&local_address));
  ASSERT_TRUE(socket.GetRemoteAddress(&remote_address));

  ASSERT_TRUE(NetAddressPrivate::AreEqual(local_address, local_address));
  ASSERT_FALSE(NetAddressPrivate::AreEqual(local_address, remote_address));

  socket.Disconnect();
  PASS();
}

std::string TestNetAddressPrivateUntrusted::TestAreHostsEqual() {
  pp::TCPSocketPrivate socket(instance_);
  int32_t rv = Connect(&socket, host_, port_);
  if (rv != PP_OK)
    return ReportError("pp::TCPSocketPrivate::Connect", rv);

  PP_NetAddress_Private local_address, remote_address;
  ASSERT_TRUE(socket.GetLocalAddress(&local_address));
  ASSERT_TRUE(socket.GetRemoteAddress(&remote_address));

  ASSERT_TRUE(NetAddressPrivate::AreHostsEqual(local_address, local_address));
  ASSERT_TRUE(NetAddressPrivate::AreHostsEqual(local_address, remote_address));

  socket.Disconnect();
  PASS();
}

std::string TestNetAddressPrivateUntrusted::TestDescribe() {
  pp::TCPSocketPrivate socket(instance_);
  int32_t rv = Connect(&socket, host_, port_);
  if (rv != PP_OK)
    return ReportError("pp::TCPSocketPrivate::Connect", rv);

  PP_NetAddress_Private remote_address;
  ASSERT_TRUE(socket.GetRemoteAddress(&remote_address));

  std::ostringstream os;

  os << host_;
  ASSERT_EQ(os.str(), NetAddressPrivate::Describe(remote_address, false));

  os << ':' << port_;
  ASSERT_EQ(os.str(), NetAddressPrivate::Describe(remote_address, true));

  socket.Disconnect();
  PASS();
}

std::string TestNetAddressPrivateUntrusted::TestReplacePort() {
  pp::TCPSocketPrivate socket(instance_);
  int32_t rv = Connect(&socket, host_, port_);
  if (rv != PP_OK)
    return ReportError("pp::TCPSocketPrivate::Connect", rv);

  PP_NetAddress_Private src_addr, dst_addr;
  ASSERT_TRUE(socket.GetRemoteAddress(&src_addr));

  uint16_t nport = port_;
  if (nport == std::numeric_limits<uint16_t>::max())
    --nport;
  else
    ++nport;
  ASSERT_TRUE(NetAddressPrivate::ReplacePort(src_addr, nport, &dst_addr));

  std::ostringstream os;
  os << host_ << ':' << nport;

  ASSERT_EQ(os.str(), NetAddressPrivate::Describe(dst_addr, true));

  socket.Disconnect();
  PASS();
}

std::string TestNetAddressPrivateUntrusted::TestGetAnyAddress() {
  PP_NetAddress_Private address;

  NetAddressPrivate::GetAnyAddress(false, &address);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(address, address));

  NetAddressPrivate::GetAnyAddress(true, &address);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(address, address));

  PASS();
}

std::string TestNetAddressPrivateUntrusted::TestGetFamily() {
  pp::TCPSocketPrivate socket(instance_);
  int32_t rv = Connect(&socket, host_, port_);
  if (rv != PP_OK)
    return ReportError("pp::TCPSocketPrivate::Connect", rv);

  PP_NetAddress_Private remote_address;
  ASSERT_TRUE(socket.GetRemoteAddress(&remote_address));

  ASSERT_EQ(NetAddressPrivate::GetFamily(remote_address),
            NetAddressPrivate::GetFamily(remote_address));

  socket.Disconnect();
  PASS();
}

std::string TestNetAddressPrivateUntrusted::TestGetPort() {
  pp::TCPSocketPrivate socket(instance_);
  int32_t rv = Connect(&socket, host_, port_);
  if (rv != PP_OK)
    return ReportError("pp::TCPSocketPrivate::Connect", rv);

  PP_NetAddress_Private remote_address;
  ASSERT_TRUE(socket.GetRemoteAddress(&remote_address));

  ASSERT_EQ(NetAddressPrivate::GetPort(remote_address), port_);

  socket.Disconnect();
  PASS();
}

std::string TestNetAddressPrivateUntrusted::TestGetAddress() {
  pp::TCPSocketPrivate socket(instance_);
  int32_t rv = Connect(&socket, host_, port_);
  if (rv != PP_OK)
    return ReportError("pp::TCPSocketPrivate::Connect", rv);

  PP_NetAddress_Private remote_address;
  ASSERT_TRUE(socket.GetRemoteAddress(&remote_address));

  static const uint16_t buffer_size = sizeof(remote_address.data);
  char buffer[buffer_size];
  ASSERT_TRUE(NetAddressPrivate::GetAddress(remote_address, buffer,
                                            buffer_size));

  socket.Disconnect();
  PASS();
}
