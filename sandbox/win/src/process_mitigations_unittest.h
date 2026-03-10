// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_UNITTEST_H_
#define SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_UNITTEST_H_

#include "sandbox/win/tests/common/controller.h"

namespace sandbox {

// Declare shared test commands.
SBOX_TEST_DECLARE_COMMAND(CheckPolicy);
SBOX_TEST_DECLARE_COMMAND(TestChildProcess);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_UNITTEST_H_
