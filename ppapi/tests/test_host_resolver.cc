// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_host_resolver.h"

#include <stddef.h>

#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(HostResolver);

TestHostResolver::TestHostResolver(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestHostResolver::Init() {
  bool host_resolver_is_available = pp::HostResolver::IsAvailable();
  if (!host_resolver_is_available)
    instance_->AppendError("PPB_HostResolver interface not available");

  bool tcp_socket_is_available = pp::TCPSocket::IsAvailable();
  if (!tcp_socket_is_available)
    instance_->AppendError("PPB_TCPSocket interface not available");

  bool init_host_port =
      GetLocalHostPort(instance_->pp_instance(), &host_, &port_);
  if (!init_host_port)
    instance_->AppendError("Can't init host and port");

  return host_resolver_is_available &&
      tcp_socket_is_available &&
      init_host_port &&
      CheckTestingInterface() &&
      EnsureRunningOverHTTP();
}

void TestHostResolver::RunTests(const std::string& filter) {
  RUN_TEST(Empty, filter);
  RUN_CALLBACK_TEST(TestHostResolver, Resolve, filter);
  RUN_CALLBACK_TEST(TestHostResolver, ResolveIPv4, filter);
}

std::string TestHostResolver::SyncConnect(
    pp::TCPSocket* socket,
    const pp::NetAddress& address) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(socket->Connect(address, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  PASS();
}

std::string TestHostResolver::SyncRead(pp::TCPSocket* socket,
                                       char* buffer,
                                       int32_t num_bytes,
                                       int32_t* bytes_read) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      socket->Read(buffer, num_bytes, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(num_bytes, callback.result());
  *bytes_read = callback.result();
  PASS();
}

std::string TestHostResolver::SyncWrite(pp::TCPSocket* socket,
                                        const char* buffer,
                                        int32_t num_bytes,
                                        int32_t* bytes_written) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      socket->Write(buffer, num_bytes, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(num_bytes, callback.result());
  *bytes_written = callback.result();
  PASS();
}

std::string TestHostResolver::CheckHTTPResponse(pp::TCPSocket* socket,
                                                const std::string& request,
                                                const std::string& response) {
  int32_t rv = 0;
  ASSERT_SUBTEST_SUCCESS(
      SyncWrite(socket, request.c_str(), static_cast<int32_t>(request.size()),
                &rv));
  std::vector<char> response_buffer(response.size());
  ASSERT_SUBTEST_SUCCESS(
      SyncRead(socket, &response_buffer[0],
               static_cast<int32_t>(response.size()), &rv));
  std::string actual_response(&response_buffer[0], rv);
  if (response != actual_response) {
    return "CheckHTTPResponse failed, expected: " + response +
        ", actual: " + actual_response;
  }
  PASS();
}

std::string TestHostResolver::SyncResolve(
    pp::HostResolver* host_resolver,
    const std::string& host,
    uint16_t port,
    const PP_HostResolver_Hint& hint) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      host_resolver->Resolve(host.c_str(), port, hint, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  PASS();
}

std::string TestHostResolver::ParameterizedTestResolve(
    const PP_HostResolver_Hint& hint) {
  pp::HostResolver host_resolver(instance_);

  ASSERT_SUBTEST_SUCCESS(
      SyncResolve(&host_resolver, "host_resolver.test", port_, hint));

  size_t size = host_resolver.GetNetAddressCount();
  ASSERT_EQ(1u, size);

  pp::NetAddress address;
  for (size_t i = 0; i < size; ++i) {
    address = host_resolver.GetNetAddress(static_cast<uint32_t>(i));
    ASSERT_NE(0, address.pp_resource());

    pp::TCPSocket socket(instance_);
    ASSERT_SUBTEST_SUCCESS(SyncConnect(&socket, address));
    ASSERT_SUBTEST_SUCCESS(CheckHTTPResponse(&socket,
                                             "GET / HTTP/1.0\r\n\r\n",
                                             "HTTP"));
    socket.Close();
  }

  address = host_resolver.GetNetAddress(static_cast<uint32_t>(size));
  ASSERT_EQ(0, address.pp_resource());
  pp::Var canonical_name = host_resolver.GetCanonicalName();
  ASSERT_TRUE(canonical_name.is_string());

  ASSERT_SUBTEST_SUCCESS(SyncResolve(&host_resolver, canonical_name.AsString(),
                                     port_, hint));
  size = host_resolver.GetNetAddressCount();
  ASSERT_TRUE(size >= 1);

  PASS();
}

std::string TestHostResolver::TestEmpty() {
  pp::HostResolver host_resolver(instance_);
  ASSERT_EQ(0, host_resolver.GetNetAddressCount());
  pp::NetAddress address = host_resolver.GetNetAddress(0);
  ASSERT_EQ(0, address.pp_resource());

  PASS();
}

std::string TestHostResolver::TestResolve() {
  PP_HostResolver_Hint hint;
  hint.family = PP_NETADDRESS_FAMILY_UNSPECIFIED;
  hint.flags = PP_HOSTRESOLVER_FLAG_CANONNAME;
  return ParameterizedTestResolve(hint);
}

std::string TestHostResolver::TestResolveIPv4() {
  PP_HostResolver_Hint hint;
  hint.family = PP_NETADDRESS_FAMILY_IPV4;
  hint.flags = PP_HOSTRESOLVER_FLAG_CANONNAME;
  return ParameterizedTestResolve(hint);
}
