// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"

#include <string_view>

namespace extensions {

StaticV8ExternalOneByteStringResource::StaticV8ExternalOneByteStringResource(
    std::string_view buffer)
    : buffer_(buffer) {}

StaticV8ExternalOneByteStringResource::
    ~StaticV8ExternalOneByteStringResource() {
}

const char* StaticV8ExternalOneByteStringResource::data() const {
  return buffer_.data();
}

size_t StaticV8ExternalOneByteStringResource::length() const {
  return buffer_.length();
}

}  // namespace extensions
