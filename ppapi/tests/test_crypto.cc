// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_crypto.h"

#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Crypto);

TestCrypto::TestCrypto(TestingInstance* instance)
    : TestCase(instance),
      crypto_interface_(NULL) {
}

bool TestCrypto::Init() {
  crypto_interface_ = static_cast<const PPB_Crypto_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CRYPTO_DEV_INTERFACE));
  return !!crypto_interface_;
}

void TestCrypto::RunTests(const std::string& filter) {
  RUN_TEST(GetRandomBytes, filter);
}

std::string TestCrypto::TestGetRandomBytes() {
  const int kBufSize = 16;
  char buf[kBufSize] = {0};

  crypto_interface_->GetRandomBytes(buf, kBufSize);

  // Verify that the interface wrote "something" to the buffer.
  bool found_nonzero = false;
  for (int i = 0; i < kBufSize; i++) {
    if (buf[i]) {
      found_nonzero = true;
      break;
    }
  }
  ASSERT_TRUE(found_nonzero);

  PASS();
}
