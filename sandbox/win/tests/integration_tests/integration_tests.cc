// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "sandbox/win/tests/common/controller.h"

namespace sandbox {
// Common function used for timeout with Win APIs like ::WaitForSingleObject().
DWORD SboxTestEventTimeout() {
  if (::IsDebuggerPresent()) {
    return INFINITE;
  }
  return static_cast<DWORD>(
      (TestTimeouts::action_timeout()).InMillisecondsRoundedUp());
}
}

int wmain(int argc, wchar_t **argv) {
  if (sandbox::IsChildProcessForTesting()) {
    // This sets default timeouts.
    TestTimeouts::Initialize();
    // This instance is a child, not the test.
    return sandbox::DispatchCall();
  }

  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv, false,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
