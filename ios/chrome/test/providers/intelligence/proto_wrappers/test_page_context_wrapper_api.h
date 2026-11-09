// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_INTELLIGENCE_PROTO_WRAPPERS_TEST_PAGE_CONTEXT_WRAPPER_API_H_
#define IOS_CHROME_TEST_PROVIDERS_INTELLIGENCE_PROTO_WRAPPERS_TEST_PAGE_CONTEXT_WRAPPER_API_H_

namespace ios::provider::test {

// Overrides the return value of `ios::provider::IsProtectedUrl()` in tests.
void SetProtectedUrlForTesting(bool is_protected);

}  // namespace ios::provider::test

#endif  // IOS_CHROME_TEST_PROVIDERS_INTELLIGENCE_PROTO_WRAPPERS_TEST_PAGE_CONTEXT_WRAPPER_API_H_
