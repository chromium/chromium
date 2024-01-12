// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_STATIC_V8_EXTERNAL_ONE_BYTE_STRING_RESOURCE_H_
#define EXTENSIONS_RENDERER_STATIC_V8_EXTERNAL_ONE_BYTE_STRING_RESOURCE_H_

#include <stddef.h>

#include <string_view>

#include "base/compiler_specific.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

// A very simple implementation of v8::ExternalAsciiStringResource that just
// wraps a buffer. The buffer must outlive the v8 runtime instance this resource
// is used in.
class StaticV8ExternalOneByteStringResource
    : public v8::String::ExternalOneByteStringResource {
 public:
  explicit StaticV8ExternalOneByteStringResource(std::string_view buffer);
  ~StaticV8ExternalOneByteStringResource() override;

  const char* data() const override;
  size_t length() const override;

 private:
  std::string_view buffer_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_STATIC_V8_EXTERNAL_ONE_BYTE_STRING_RESOURCE_H_
