// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_TRUSTED_H_
#define PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_TRUSTED_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

// This class is necessary to test the portions of TCP socket which are
// not exposed to NaCl yet. In particular, functionality related to
// X509 Certificates is tested here.
class TestTCPSocketPrivateTrusted : public TestCase {
 public:
  explicit TestTCPSocketPrivateTrusted(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestGetServerCertificate();

  std::string host_;
  uint16_t port_;
  uint16_t ssl_port_;
};

#endif  // PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_TRUSTED_H_
