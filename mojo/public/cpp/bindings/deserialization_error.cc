// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/deserialization_error.h"

#include "base/strings/stringprintf.h"

namespace mojo {

std::string DeserializationError::ToString() const {
  if (custom_code_) {
    return base::StringPrintf("%s [%d]", location_.ToString().c_str(),
                              *custom_code_);
  }
  return location_.ToString();
}

}  // namespace mojo
