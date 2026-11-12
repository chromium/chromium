// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ENUM_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ENUM_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"

namespace mojo::internal {

// Formats an error string for an unknown enum value.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
std::string FormatUnknownEnumValue(const char* enum_name, int32_t value);

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ENUM_UTIL_H_
