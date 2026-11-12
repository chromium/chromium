// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/enum_util.h"

#include <stdint.h>

#include <string>

#include "base/strings/stringprintf.h"

namespace mojo::internal {

std::string FormatUnknownEnumValue(const char* enum_name, int32_t value) {
  return base::StringPrintf("Unknown %s value: %i", enum_name, value);
}

}  // namespace mojo::internal
