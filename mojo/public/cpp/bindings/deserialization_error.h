// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DESERIALIZATION_ERROR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DESERIALIZATION_ERROR_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/location.h"

namespace mojo {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) DeserializationError {
 public:
  constexpr explicit DeserializationError(
      const base::Location& location = base::Location::Current())
      : location_(location) {}

  static constexpr DeserializationError CustomCode(
      int code,
      const base::Location& location = base::Location::Current()) {
    DeserializationError error(location);
    error.custom_code_ = code;
    return error;
  }

  constexpr const base::Location& location() const { return location_; }
  constexpr std::optional<int> custom_code() const { return custom_code_; }

  std::string ToString() const;

 private:
  base::Location location_;
  std::optional<int> custom_code_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DESERIALIZATION_ERROR_H_
