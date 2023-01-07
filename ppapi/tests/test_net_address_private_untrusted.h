// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_NET_ADDRESS_PRIVATE_UNTRUSTED_H_
#define PPAPI_TESTS_TEST_NET_ADDRESS_PRIVATE_UNTRUSTED_H_

#include <stdint.h>

#include <string>

#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/tests/test_case.h"

// TestNetAddressPrivate doesn't compile via NaCl toolchain, because
// these tests depend on network API which is not available in
// NaCl. TestNetAddressPrivateUntrusted is written only for check that
// API is correctly exposed to NaCl, not for checking correctness of
// API --- this is a job of TestNetAddressPrivate.
class TestNetAddressPrivateUntrusted : public TestCase {
 public:
  explicit TestNetAddressPrivateUntrusted(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  int32_t Connect(pp::TCPSocketPrivate* socket,
                  const std::string& host,
                  uint16_t port);

  std::string TestAreEqual();
  std::string TestAreHostsEqual();
  std::string TestDescribe();
  std::string TestReplacePort();
  std::string TestGetAnyAddress();
  std::string TestGetFamily();
  std::string TestGetPort();
  std::string TestGetAddress();

  std::string host_;
  uint16_t port_;
};

#endif  // PPAPI_TESTS_TEST_NET_ADDRESS_PRIVATE_UNTRUSTED_H_
