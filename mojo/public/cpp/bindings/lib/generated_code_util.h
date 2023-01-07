// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_GENERATED_CODE_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_GENERATED_CODE_UTIL_H_

#include <utility>
#include "base/component_export.h"
#include "base/containers/span.h"

namespace mojo {

class Message;

namespace internal {

class ValidationContext;

struct GenericValidationInfo {
  // Non-null, unless this corresponds to a non-existent method.
  bool (*request_validator)(const void* data, ValidationContext*);

  // Non-null, unless this corresponds to a method that does not expect a
  // response.
  bool (*response_validator)(const void* data, ValidationContext*);
};

// Provides a generic implementation of the Mojo IPC request validation,
// allowing callers to do a compact tail call in generated code.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
bool ValidateRequestGeneric(
    Message* message,
    const char* class_name,
    base::span<const std::pair<uint32_t, GenericValidationInfo>>);

// As above, but assumes that the ordinals (names) are packed such that a
// constant-time indexed table access is sufficient.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
bool ValidateRequestGenericPacked(Message* message,
                                  const char* class_name,
                                  base::span<const GenericValidationInfo>);

// Provides a generic implementation of the Mojo IPC response validation,
// allowing callers to do a compact tail call in generated code.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
bool ValidateResponseGeneric(
    Message* message,
    const char* class_name,
    base::span<const std::pair<uint32_t, GenericValidationInfo>>);

// As above, but assumes that the ordinals (names) are packed such that a
// constant-time indexed table access is sufficient.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
bool ValidateResponseGenericPacked(Message* message,
                                   const char* class_name,
                                   base::span<const GenericValidationInfo>);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_GENERATED_CODE_UTIL_H_
