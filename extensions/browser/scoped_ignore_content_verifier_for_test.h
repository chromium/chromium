// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCOPED_IGNORE_CONTENT_VERIFIER_FOR_TEST_H_
#define EXTENSIONS_BROWSER_SCOPED_IGNORE_CONTENT_VERIFIER_FOR_TEST_H_

namespace extensions {

// A class for use in tests to make content verification failures be ignored
// during the lifetime of an instance of it. Note that only one instance should
// be alive at any given time.
class ScopedIgnoreContentVerifierForTest {
 public:
  ScopedIgnoreContentVerifierForTest();

  ScopedIgnoreContentVerifierForTest(
      const ScopedIgnoreContentVerifierForTest&) = delete;
  ScopedIgnoreContentVerifierForTest& operator=(
      const ScopedIgnoreContentVerifierForTest&) = delete;

  ~ScopedIgnoreContentVerifierForTest();
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCOPED_IGNORE_CONTENT_VERIFIER_FOR_TEST_H_
