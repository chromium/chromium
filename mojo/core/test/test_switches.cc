// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/test/test_switches.h"

namespace test_switches {

// Forces the process's global Mojo node to be configured as a broker. Only
// honored for test runners using MojoTestSuiteBase.
const char kMojoIsBroker[] = "mojo-is-broker";

}  // namespace test_switches
