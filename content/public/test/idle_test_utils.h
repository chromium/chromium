// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_IDLE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_IDLE_TEST_UTILS_H_

#include <memory>

namespace content {

class IdleTimeProvider;

class ScopedIdleProviderForTest {
 public:
  explicit ScopedIdleProviderForTest(
      std::unique_ptr<IdleTimeProvider> idle_time_provider);
  ~ScopedIdleProviderForTest();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_IDLE_TEST_UTILS_H_
