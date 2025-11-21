// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/validation_errors_test_util.h"

#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo::internal {

SerializationWarningObserverForTesting::SerializationWarningObserverForTesting()
    : set_values_callback_(base::BindRepeating(
          &SerializationWarningObserverForTesting::SetValues,
          base::Unretained(this))) {
  SetSerializationWarningCallbackForTesting(&set_values_callback_);
}

SerializationWarningObserverForTesting::
    ~SerializationWarningObserverForTesting() {
  SetSerializationWarningCallbackForTesting(nullptr);
}

ValidationErrorObserverForTesting::ValidationErrorObserverForTesting(
    base::RepeatingClosure callback)
    : set_validation_error_callback_(base::BindRepeating(
          &ValidationErrorObserverForTesting::set_last_error,
          base::Unretained(this))),
      callback_(std::move(callback)) {
  SetValidationErrorCallbackForTesting(&set_validation_error_callback_);
}

ValidationErrorObserverForTesting::~ValidationErrorObserverForTesting() {
  SetValidationErrorCallbackForTesting(nullptr);
}

}  // namespace mojo::internal
