// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

// Let's not use any of the "magic" values used internally in unit_tests.cc,
// such as kExpectedValue.
const int kExpectedExitCode = 100;

SANDBOX_DEATH_TEST(UnitTests,
                   DeathExitCode,
                   DEATH_EXIT_CODE(kExpectedExitCode)) {
  _exit(kExpectedExitCode);
}

const int kExpectedSignalNumber = SIGKILL;

SANDBOX_DEATH_TEST(UnitTests,
                   DeathBySignal,
                   DEATH_BY_SIGNAL(kExpectedSignalNumber)) {
  raise(kExpectedSignalNumber);
}

SANDBOX_DEATH_TEST(UnitTests,
                   DeathWithMessage,
                   DEATH_MESSAGE("Hello")) {
  LOG(ERROR) << "Hello";
  _exit(1);
}

SANDBOX_DEATH_TEST(UnitTests,
                   SEGVDeathWithMessage,
                   DEATH_SEGV_MESSAGE("Hello")) {
  LOG(ERROR) << "Hello";
  while (true) {
    volatile char* addr = reinterpret_cast<volatile char*>(NULL);
    *addr = '\0';
  }
}

SANDBOX_TEST_ALLOW_NOISE(UnitTests, NoisyTest) {
  LOG(ERROR) << "The cow says moo!";
}

}  // namespace

}  // namespace sandbox
