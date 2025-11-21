// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_TEST_SUPPORT_VALIDATION_ERRORS_TEST_UTIL_H_
#define MOJO_PUBLIC_CPP_TEST_SUPPORT_VALIDATION_ERRORS_TEST_UTIL_H_

#include <optional>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/lib/send_validation_type.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo::internal {

// Only used by serialization tests and when there is only one thread doing
// message serialization.
class SerializationWarningObserverForTesting {
 public:
  SerializationWarningObserverForTesting();

  SerializationWarningObserverForTesting(
      const SerializationWarningObserverForTesting&) = delete;
  SerializationWarningObserverForTesting& operator=(
      const SerializationWarningObserverForTesting&) = delete;

  ~SerializationWarningObserverForTesting();

  ValidationError last_warning() const { return last_warning_; }
  std::optional<SendValidation> send_side_validation() const {
    return send_validation_type_;
  }
  void set_last_warning(ValidationError error) { last_warning_ = error; }
  void clear_send_validation() { send_validation_type_ = std::nullopt; }

 private:
  void SetValues(ValidationError error,
                 SendValidation send_side_validation_type) {
    last_warning_ = error;
    send_validation_type_ = send_side_validation_type;
  }

  base::RepeatingCallback<void(ValidationError, SendValidation)>
      set_values_callback_;

  ValidationError last_warning_ = VALIDATION_ERROR_NONE;
  std::optional<SendValidation> send_validation_type_;
};

// Only used by validation tests and when there is only one thread doing message
// validation.
class ValidationErrorObserverForTesting {
 public:
  explicit ValidationErrorObserverForTesting(base::RepeatingClosure callback);

  ValidationErrorObserverForTesting(const ValidationErrorObserverForTesting&) =
      delete;
  ValidationErrorObserverForTesting& operator=(
      const ValidationErrorObserverForTesting&) = delete;

  ~ValidationErrorObserverForTesting();

  ValidationError last_error() const { return last_error_; }

 private:
  void set_last_error(ValidationError error) {
    last_error_ = error;
    callback_.Run();
  }

  base::RepeatingCallback<void(ValidationError)> set_validation_error_callback_;

  ValidationError last_error_ = VALIDATION_ERROR_NONE;
  base::RepeatingClosure callback_;
};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_TEST_SUPPORT_VALIDATION_ERRORS_TEST_UTIL_H_
