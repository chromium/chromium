// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_apitest.h"

namespace extensions {

// A placeholder browser test to keep the extensions_browsertest binary from
// being empty. This test does nothing and can be removed when the
// extensions_browsertest binary is removed.
class PlaceholderBrowsertest : public ShellApiTest {
 public:
  PlaceholderBrowsertest() = default;

  PlaceholderBrowsertest(const PlaceholderBrowsertest&) = delete;
  PlaceholderBrowsertest& operator=(const PlaceholderBrowsertest&) = delete;

  ~PlaceholderBrowsertest() override = default;
};

IN_PROC_BROWSER_TEST_F(PlaceholderBrowsertest, Placeholder) {
  // This space intentionally left blank.
}

}  // namespace extensions
