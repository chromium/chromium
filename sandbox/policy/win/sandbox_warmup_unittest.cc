// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_warmup.h"

#include <windows.h>

#include <dwrite.h>
#include <wrl/client.h>

#include "base/rand_util.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

SBOX_TESTS_COMMAND int WarmupRandomness(int argc, wchar_t** argv) {
  static int state = BEFORE_INIT;

  switch (state++) {
    case BEFORE_INIT: {
      // Warmup.
      sandbox::policy::WarmupRandomnessInfrastructure();
      return SBOX_TEST_SUCCEEDED;
    }
    case AFTER_REVERT: {
      const auto bytes = base::RandBytesAsVector(23);
      EXPECT_EQ(bytes.size(), 23u);
      return SBOX_TEST_SUCCEEDED;
    }
    default:  // Do nothing.
      break;
  }
  return SBOX_TEST_SUCCEEDED;
}

// Do something with Dwrite.dll to validate that the IAT hook used for Csrss
// applies without crashing. The hooked call within dwrite.dll is not
// straightforward to trigger, so this test mainly validates that the import to
// hook is present.
SBOX_TESTS_COMMAND int HookDwrite(int argc, wchar_t** argv) {
  static int state = BEFORE_INIT;
  static HMODULE dwrite_mod = nullptr;

  switch (state++) {
    case BEFORE_INIT: {
      // Load dwrite.dll so that the IATHook is applied, keep the handle.
      dwrite_mod = ::LoadLibraryW(L"dwrite.dll");
      CHECK(dwrite_mod);

      // Apply the hook. This may fail if dwrite's imports change in future
      // versions of Windows.
      CHECK(sandbox::policy::HookDwriteGetUserDefaultLCID());

      return SBOX_TEST_SUCCEEDED;
    }
    case AFTER_REVERT: {
      // Use dwrite.dll.
      decltype(DWriteCreateFactory)* DWriteCreateFactoryFn =
          reinterpret_cast<decltype(DWriteCreateFactory)*>(
              ::GetProcAddress(dwrite_mod, "DWriteCreateFactory"));
      CHECK(DWriteCreateFactoryFn);

      {
        Microsoft::WRL::ComPtr<IUnknown> dwrite_factory;
        HRESULT hr =
            DWriteCreateFactoryFn(DWRITE_FACTORY_TYPE_SHARED,
                                  __uuidof(IDWriteFactory), &dwrite_factory);
        CHECK(hr == S_OK);
      }

      // Should be safe to unload dwrite.dll without crashing.
      ::FreeLibrary(dwrite_mod);
      return SBOX_TEST_SUCCEEDED;
    }
    default:  // Do nothing.
      break;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(SandboxPolicyWarmup, Randomness) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"WarmupRandomness"));
}

TEST(SandboxPolicyWarmup, HookDwrite) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"HookDwrite"));
}

}  // namespace sandbox
