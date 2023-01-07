// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_EXTENSION_STATE_TESTER_H_
#define EXTENSIONS_TEST_EXTENSION_STATE_TESTER_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionPrefs;
class ExtensionRegistry;

// A utility class to help check expected extension states (enabled, disabled,
// etc). Generally prefer this class to direct checks on the ExtensionRegistry.
//
// This class will do more robust checking than just checking the
// associated ExtensionRegistry set like so:
//   ExtensionRegistry* registry = GetRegistry();
//   EXPECT_TRUE(registry->disabled_extensions().Contains(id));
// Unlike the above code, ExtensionStateTester will validate other assumptions,
// including that the extension is *only* in a single registry set, that it is
// disabled with the proper reason(s), etc.
//
// Each method will emit detailed failures to the test via gtest's ADD_FAILURE()
// for failed assumptions. There may be multiple failed assumptions for a given
// method call, which can help in diagnosing the current state versus the
// expected state. For instance, a call to ExpectEnabled() with a disabled
// extension will log that:
// - The extension is not in the enabled set
// - The extension is unexpectedly in the disabled set
// - The extension had unexpected disable reasons
// Since these failures are added from the body of these functions, they do not
// include the line number of the calling function. To pinpoint which call
// failed when multiple calls are in a single test (not uncommon), callers
// should EXPECT_TRUE() on the return value of each function call. This way,
// there will also be a failure listed for the immediate callsite, highlighting
// which call to the method failed.
//
// Example usage:
//   ExtensionStateTester state_tester(browser_context());
//   EXPECT_TRUE(state_tester.ExpectEnabled(id));
//   DisableExtension(id, disable_reason::DISABLE_USER_ACTION);
//   EXPECT_TRUE(state_tester.ExpectDisabledWithSingleReason(
//       id, disable_reason::DISABLE_USER_ACTION));
//   ...
//
// You should never use EXPECT_FALSE() on one of these methods; instead, call
// the appropriate method for the expectation. For instance, use
//   EXPECT_TRUE(state_tester.ExpectBlocklisted(id));
// rather than
//   EXPECT_FALSE(state_tester.ExpectEnabled(id));
// Though ExpectEnabled() *will* return false if the extension is not enabled,
// it will also add (potentially multiple) failures to the active test.
class ExtensionStateTester {
 public:
  explicit ExtensionStateTester(content::BrowserContext* browser_context);
  ExtensionStateTester(const ExtensionStateTester&) = delete;
  ExtensionStateTester& operator=(const ExtensionStateTester&) = delete;
  ~ExtensionStateTester();

  // Expects the extension to be enabled.
  [[nodiscard]] bool ExpectEnabled(const ExtensionId& extension_id);

  // Expects the extension to be disabled with (only) the specified `reason`.
  [[nodiscard]] bool ExpectDisabledWithSingleReason(
      const ExtensionId& extension_id,
      disable_reason::DisableReason reason);

  // Expects the extension to be disabled with exactly the specified
  // `disable_reasons`.
  [[nodiscard]] bool ExpectDisabledWithReasons(const ExtensionId& extension_id,
                                               int disable_reasons);

  // Expects the extension to be blocklisted.
  [[nodiscard]] bool ExpectBlocklisted(const ExtensionId& extension_id);

  // Expects the extension to be terminated.
  [[nodiscard]] bool ExpectTerminated(const ExtensionId& extension_id);

 private:
  // Helper method to iterate over the registry sets, expecting the extension
  // to only be within the one indicated by `set_name`.
  [[nodiscard]] bool ExpectOnlyInSet(const ExtensionId& extension_id,
                                     const char* set_name);

  const raw_ptr<ExtensionRegistry> registry_;
  const raw_ptr<ExtensionPrefs> prefs_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_EXTENSION_STATE_TESTER_H_
