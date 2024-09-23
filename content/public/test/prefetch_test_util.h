// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PREFETCH_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_PREFETCH_TEST_UTIL_H_

#include <memory>

#include "base/types/strong_alias.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content::test {

using PrefetchContainerIdForTesting =
    base::StrongAlias<class PrefetchContainerIdForTestingTag, std::string>;
static constexpr PrefetchContainerIdForTesting
    InvalidPrefetchContainerIdForTesting = PrefetchContainerIdForTesting("");

class TestPrefetchWatcherImpl;

// A test helper to observe prefetch behaviors.
// TODO(crbug.com/40946257): There is a room to revisit the current test
// interface per upcoming changes and our needs of what properties we want to
// test. See discussion on crrev.com/c/5455871/comment/04ee743c_19b686db/.
class TestPrefetchWatcher {
 public:
  TestPrefetchWatcher();
  ~TestPrefetchWatcher();

  TestPrefetchWatcher(const TestPrefetchWatcher&) = delete;
  TestPrefetchWatcher& operator=(const TestPrefetchWatcher&) = delete;

  // Waits until the specific prefetch request searched by its
  // document token and url (which correspond the properties of
  // `PrefetchContainer::Key`) is successfully completed. Returns a
  // test-specific id for `PrefetchContainer`.
  PrefetchContainerIdForTesting WaitUntilPrefetchResponseCompleted(
      const std::optional<blink::DocumentToken>& document_token,
      const GURL& url);

  // Returns whether prefetch was served in the last navigation. Returns
  // std::nullopt if the first navigation hasn't happened yet.
  std::optional<bool> PrefetchUsedInLastNavigation();

  // Returns a test-specific id of `PrefetchContainer`, if the prefetch was
  // served in the last navigation. Returns
  // `InvalidPrefetchContainerIdForTesting` if not served. Returns std::nullopt
  // if the first navigation hasn't happened yet.
  std::optional<PrefetchContainerIdForTesting>
  GetPrefetchContainerIdForTestingInLastNavigation();

 private:
  std::unique_ptr<TestPrefetchWatcherImpl> impl_;
};

}  // namespace content::test

#endif  // CONTENT_PUBLIC_TEST_PREFETCH_TEST_UTIL_H_
