// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CRYPTO_H_
#define PPAPI_TESTS_TEST_CRYPTO_H_

#include <string>

#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/tests/test_case.h"

class TestCrypto : public TestCase {
 public:
  TestCrypto(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestGetRandomBytes();

  const PPB_Crypto_Dev* crypto_interface_;
};

#endif  // PPAPI_TESTS_TEST_CRYPTO_H_
