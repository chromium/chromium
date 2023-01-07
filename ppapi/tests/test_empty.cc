// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_empty.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Empty);
// This test checks only that the NaCl module loads successfully.
// This is needed when the NaCl module is launched under the debugger and
// so we want to check only the launching sequence.
TestEmpty::TestEmpty(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestEmpty::Init() {
  return true;
}

void TestEmpty::RunTests(const std::string& filter) {
  RUN_TEST(NaClLoad, filter);
}

std::string TestEmpty::TestNaClLoad() {
  PASS();
}
