// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UDP_SOCKET_H_
#define PPAPI_TESTS_TEST_UDP_SOCKET_H_

#include <stddef.h>

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/tests/test_case.h"

namespace {
typedef int32_t (*UDPSocketSetOption)(PP_Resource udp_socket,
                                      PP_UDPSocket_Option name,
                                      struct PP_Var value,
                                      struct PP_CompletionCallback callback);
}

namespace pp {
class UDPSocket;
}

class TestUDPSocket: public TestCase {
 public:
  explicit TestUDPSocket(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string GetLocalAddress(pp::NetAddress* address);
  std::string SetBroadcastOptions(pp::UDPSocket* socket);
  std::string BindUDPSocket(pp::UDPSocket* socket,
                            const pp::NetAddress& address);
  std::string LookupPortAndBindUDPSocket(pp::UDPSocket* socket,
                                         pp::NetAddress* address);
  std::string ReadSocket(pp::UDPSocket* socket,
                         pp::NetAddress* address,
                         size_t size,
                         std::string* message);
  std::string PassMessage(pp::UDPSocket* target,
                          pp::UDPSocket* source,
                          const pp::NetAddress& target_address,
                          const std::string& message,
                          pp::NetAddress* recvfrom_address);
  std::string SetMulticastOptions(pp::UDPSocket* socket);

  std::string TestReadWrite();
  std::string TestBroadcast();
  int32_t SetOptionValue(UDPSocketSetOption func,
                         PP_Resource socket,
                         PP_UDPSocket_Option option,
                         const PP_Var& value);
  std::string TestSetOption_1_0();
  std::string TestSetOption_1_1();
  std::string TestSetOption();
  std::string TestParallelSend();
  std::string TestMulticast();

  // Error cases. It's up to the parent test fixture to ensure that these events
  // result in errors.
  std::string TestBindFails();
  std::string TestSetBroadcastFails();
  std::string TestSendToFails();
  std::string TestReadFails();

  pp::NetAddress address_;

  const PPB_UDPSocket_1_0* socket_interface_1_0_;
  const PPB_UDPSocket_1_1* socket_interface_1_1_;
};

#endif  // PPAPI_TESTS_TEST_UDP_SOCKET_H_
