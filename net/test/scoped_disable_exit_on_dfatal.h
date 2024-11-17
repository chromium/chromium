// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SCOPED_DISABLE_EXIT_ON_DFATAL_H_
#define NET_TEST_SCOPED_DISABLE_EXIT_ON_DFATAL_H_

#include <string_view>

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

// The ScopedDisableExitOnDFatal class is used to disable exiting the
// program when we encounter a LOG(DFATAL) within the current block.
// After we leave the current block, the default behavior is
// restored.
class ScopedDisableExitOnDFatal {
 public:
  ScopedDisableExitOnDFatal();

  ScopedDisableExitOnDFatal(const ScopedDisableExitOnDFatal&) = delete;
  ScopedDisableExitOnDFatal& operator=(const ScopedDisableExitOnDFatal&) =
      delete;

  ~ScopedDisableExitOnDFatal();

 private:
  // Static function which is set as the logging assert handler.
  // Called when there is a check failure.
  static void LogAssertHandler(const char* file,
                               int line,
                               std::string_view message,
                               std::string_view stack_trace);

  logging::ScopedLogAssertHandler assert_handler_;
};

}  // namespace net::test

#endif  // NET_TEST_SCOPED_DISABLE_EXIT_ON_DFATAL_H_
