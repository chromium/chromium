// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/chromeos_remoting_test_suite.h"

#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

ChromeOSRemotingTestSuite::ChromeOSRemotingTestSuite(int argc, char** argv)
    : ash::AshTestSuite(argc, argv) {}

ChromeOSRemotingTestSuite::~ChromeOSRemotingTestSuite() = default;

void ChromeOSRemotingTestSuite::Initialize() {
  ash::AshTestSuite::Initialize();
}

void ChromeOSRemotingTestSuite::Shutdown() {
  ash::AshTestSuite::Shutdown();
}

}  // namespace remoting
