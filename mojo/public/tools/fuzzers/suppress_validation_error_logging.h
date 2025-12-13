// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TOOLS_FUZZERS_SUPPRESS_VALIDATION_ERROR_LOGGING_H_
#define MOJO_PUBLIC_TOOLS_FUZZERS_SUPPRESS_VALIDATION_ERROR_LOGGING_H_

namespace mojo::internal {

// This class may be used by tests to suppress validation error logging. This is
// not thread-safe and must only be instantiated on the main thread with no
// other threads using Mojo bindings at the time of construction or destruction.
class ScopedSuppressValidationErrorLoggingForTests {
 public:
  ScopedSuppressValidationErrorLoggingForTests();
  ScopedSuppressValidationErrorLoggingForTests(
      const ScopedSuppressValidationErrorLoggingForTests&) = delete;
  ScopedSuppressValidationErrorLoggingForTests& operator=(
      const ScopedSuppressValidationErrorLoggingForTests&) = delete;
  ~ScopedSuppressValidationErrorLoggingForTests();

 private:
  const bool was_suppressed_;
};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_TOOLS_FUZZERS_SUPPRESS_VALIDATION_ERROR_LOGGING_H_
