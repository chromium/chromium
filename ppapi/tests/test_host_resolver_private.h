// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_H_
#define PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_H_

#include <stdint.h>

#include <string>

#include "ppapi/tests/test_case.h"

struct PP_HostResolver_Private_Hint;
struct PP_NetAddress_Private;

namespace pp {

class HostResolverPrivate;
class TCPSocketPrivate;

}  // namespace pp

class TestHostResolverPrivate : public TestCase {
 public:
  explicit TestHostResolverPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string SyncConnect(pp::TCPSocketPrivate* socket,
                          const std::string& host,
                          uint16_t port);
  std::string SyncConnect(pp::TCPSocketPrivate* socket,
                          const PP_NetAddress_Private& address);
  std::string SyncRead(pp::TCPSocketPrivate* socket,
                       char* buffer,
                       int32_t num_bytes,
                       int32_t* bytes_read);
  std::string SyncWrite(pp::TCPSocketPrivate* socket,
                        const char* buffer,
                        int32_t num_bytes,
                        int32_t* bytes_written);
  std::string CheckHTTPResponse(pp::TCPSocketPrivate* socket,
                                const std::string& request,
                                const std::string& response);
  std::string SyncResolve(pp::HostResolverPrivate* host_resolver,
                          const std::string& host,
                          uint16_t port,
                          const PP_HostResolver_Private_Hint& hint);
  std::string ParametrizedTestResolve(const PP_HostResolver_Private_Hint& hint);

  std::string TestEmpty();
  std::string TestResolve();
  std::string TestResolveIPv4();

  std::string host_;
  uint16_t port_;
};

#endif  // PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_H_
