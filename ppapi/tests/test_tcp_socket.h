// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_TCP_SOCKET_H_
#define PAPPI_TESTS_TEST_TCP_SOCKET_H_

#include <stddef.h>

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/tests/test_case.h"

namespace pp {
class TCPSocket;
}

class TestTCPSocket: public TestCase {
 public:
  explicit TestTCPSocket(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestConnect();
  std::string TestReadWrite();
  std::string TestSetOption();
  std::string TestListen();
  std::string TestBacklog();
  std::string TestInterface_1_0();
  std::string TestUnexpectedCalls();

  // The higher level test fixture is responsible for making socket methods
  // behave in the expected manner.  The *Fails tests expect the specified even
  // to fail with PP_ERROR_FAILED, and the *Hangs test expect the specified
  // operation to never complete, at least until teardown starts.
  std::string TestConnectFails();
  std::string TestConnectHangs();
  std::string TestWriteFails();
  std::string TestReadFails();
  std::string TestSetSendBufferSizeFails();
  std::string TestSetReceiveBufferSizeFails();
  std::string TestSetNoDelayFails();
  // When a bind call fails, normally the socket is reuseable.
  std::string TestBindFailsConnectSucceeds();
  // This is needed in addition to the above test in the case where a bind
  // failure is simulated in a way that also closes the NetworkContext pipe.
  std::string TestBindFails();
  std::string TestBindHangs();
  std::string TestListenFails();
  std::string TestListenHangs();
  std::string TestAcceptFails();
  std::string TestAcceptHangs();
  std::string TestAcceptedSocketWriteFails();
  std::string TestAcceptedSocketReadFails();
  std::string TestBindConnectFails();
  std::string TestBindConnectHangs();

  std::string ReadFirstLineFromSocket(pp::TCPSocket* socket, std::string* s);
  std::string ReadFirstLineFromSocket_1_0(PP_Resource socket,
                                          std::string* s);
  // Expects to read exactly |num_bytes| from the socket. Stops once exactly
  // |num_bytes| have been read.
  std::string ReadFromSocket(pp::TCPSocket* socket,
                             char* buffer,
                             size_t num_bytes);
  // Reads from |socket| until a read error occurs, and sets |read_data| and
  // |error| accordingly. Only fails if a Read() call returns more data than the
  // buffer that was passed in to it.
  std::string ReadFromSocketUntilError(pp::TCPSocket* socket,
                                       std::string* read_data,
                                       int* error);
  std::string WriteToSocket(pp::TCPSocket* socket, const std::string& s);
  std::string WriteToSocket_1_0(PP_Resource socket, const std::string& s);

  // Sets |address| to an address usable for Bind(). Returned address uses the
  // IP address used to talk to |test_server_addr_| and a port of 0, so Bind()
  // calls to it should succeed.
  std::string GetAddressToBind(pp::NetAddress* address);

  std::string StartListen(pp::TCPSocket* socket, int32_t backlog);

  enum Command {
    kBind = 0x1,
    kListen = 0x2,
    kAccept = 0x4,
    kConnect = 0x8,
    kReadWrite = 0x10,
    kAllCommands = -1,
  };
  // Runs |commands|, consisting of one or more Command values on |socket|,
  // expecting all of them to fail with PP_ERROR_FAILED. Useful for testing
  // invalid state transitions.
  std::string RunCommandsExpendingFailures(pp::TCPSocket* socket, int commands);

  // Address of the EmbeddedTestServer set up by the browser test fixture.
  pp::NetAddress test_server_addr_;

  const PPB_TCPSocket_1_0* socket_interface_1_0_;
};

#endif  // PAPPI_TESTS_TEST_TCP_SOCKET_H_
