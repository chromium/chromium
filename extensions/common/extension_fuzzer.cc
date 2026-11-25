// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

namespace extensions {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view key_contents(reinterpret_cast<const char*>(data), size);
  std::string key_bytes;
  Extension::ParsePEMKeyBytes(key_contents, &key_bytes);

  return 0;
}

}  // namespace extensions
