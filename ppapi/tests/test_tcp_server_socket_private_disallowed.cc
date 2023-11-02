// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_server_socket_private_disallowed.h"

#include <cstddef>

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(TCPServerSocketPrivateDisallowed);

TestTCPServerSocketPrivateDisallowed::TestTCPServerSocketPrivateDisallowed(
    TestingInstance* instance)
    : TestCase(instance),
      core_interface_(NULL),
      tcp_server_socket_private_interface_(NULL) {
}

bool TestTCPServerSocketPrivateDisallowed::Init() {
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  if (!core_interface_)
    instance_->AppendError("PPB_Core interface not available");

  tcp_server_socket_private_interface_ =
      static_cast<const PPB_TCPServerSocket_Private*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE));
  if (!tcp_server_socket_private_interface_) {
    instance_->AppendError(
        "PPB_TCPServerSocket_Private interface not available");
  }

  bool net_address_private_is_available = pp::NetAddressPrivate::IsAvailable();
  if (!net_address_private_is_available)
    instance_->AppendError("PPB_NetAddress_Private interface not available");

  return core_interface_ &&
      tcp_server_socket_private_interface_ &&
      net_address_private_is_available &&
      CheckTestingInterface();
}

void TestTCPServerSocketPrivateDisallowed::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestTCPServerSocketPrivateDisallowed, Listen, filter);
}

std::string TestTCPServerSocketPrivateDisallowed::TestListen() {
  PP_Resource socket =
      tcp_server_socket_private_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(socket != 0);
  ASSERT_TRUE(tcp_server_socket_private_interface_->IsTCPServerSocket(socket));

  PP_NetAddress_Private base_address, address;
  pp::NetAddressPrivate::GetAnyAddress(false, &base_address);
  ASSERT_TRUE(pp::NetAddressPrivate::ReplacePort(
      base_address, 0, &address));
  TestCompletionCallback callback(instance_->pp_instance());
  callback.WaitForResult(tcp_server_socket_private_interface_->Listen(
      socket,
      &address,
      1,
      callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_NE(PP_OK, callback.result());
  PASS();
}
