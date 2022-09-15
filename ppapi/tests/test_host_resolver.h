// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_HOST_RESOLVER_H_
#define PPAPI_TESTS_TEST_HOST_RESOLVER_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/tests/test_case.h"

namespace pp {
class HostResolver;
class NetAddress;
class TCPSocket;
}  // namespace pp

class TestHostResolver : public TestCase {
 public:
  explicit TestHostResolver(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string SyncConnect(pp::TCPSocket* socket,
                          const pp::NetAddress& address);
  std::string SyncRead(pp::TCPSocket* socket,
                       char* buffer,
                       int32_t num_bytes,
                       int32_t* bytes_read);
  std::string SyncWrite(pp::TCPSocket* socket,
                        const char* buffer,
                        int32_t num_bytes,
                        int32_t* bytes_written);
  std::string CheckHTTPResponse(pp::TCPSocket* socket,
                                const std::string& request,
                                const std::string& response);
  std::string SyncResolve(pp::HostResolver* host_resolver,
                          const std::string& host,
                          uint16_t port,
                          const PP_HostResolver_Hint& hint);
  std::string ParameterizedTestResolve(const PP_HostResolver_Hint& hint);

  std::string TestEmpty();
  std::string TestResolve();
  std::string TestResolveIPv4();

  std::string host_;
  uint16_t port_;
};

#endif  // PPAPI_TESTS_TEST_HOST_RESOLVER_H_
