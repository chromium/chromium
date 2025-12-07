// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tools/fuzzers/suppress_validation_error_logging.h"

#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo::internal {

ScopedSuppressValidationErrorLoggingForTests::
    ScopedSuppressValidationErrorLoggingForTests()
    : was_suppressed_(GetIsValidationErrorLoggingSuppressedForTesting()) {
  SetIsValidationErrorLoggingSuppressedForTesting(true);
}

ScopedSuppressValidationErrorLoggingForTests::
    ~ScopedSuppressValidationErrorLoggingForTests() {
  SetIsValidationErrorLoggingSuppressedForTesting(was_suppressed_);
}

}  // namespace mojo::internal
