// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_udp_socket_private_disallowed.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/tests/test_utils.h"

REGISTER_TEST_CASE(UDPSocketPrivateDisallowed);

TestUDPSocketPrivateDisallowed::TestUDPSocketPrivateDisallowed(
    TestingInstance* instance)
    : TestCase(instance), udp_socket_private_interface_(NULL) {
}

bool TestUDPSocketPrivateDisallowed::Init() {
  udp_socket_private_interface_ = static_cast<const PPB_UDPSocket_Private*>(
      pp::Module::Get()->GetBrowserInterface(PPB_UDPSOCKET_PRIVATE_INTERFACE));
  if (!udp_socket_private_interface_)
    instance_->AppendError("UDPSocketPrivate interface not available");
  return udp_socket_private_interface_ && CheckTestingInterface();
}

void TestUDPSocketPrivateDisallowed::RunTests(const std::string& filter) {
  RUN_TEST(Bind, filter);
}

std::string TestUDPSocketPrivateDisallowed::TestBind() {
  PP_Resource socket =
      udp_socket_private_interface_->Create(instance_->pp_instance());
  if (0 != socket) {
    PP_NetAddress_Private addr;
    pp::NetAddressPrivate::GetAnyAddress(false, &addr);

    TestCompletionCallback callback(instance_->pp_instance());
    callback.WaitForResult(udp_socket_private_interface_->Bind(socket, &addr,
        callback.GetCallback().pp_completion_callback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  }
  PASS();
}
