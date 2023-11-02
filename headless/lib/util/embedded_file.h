// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_UTIL_EMBEDDED_FILE_H_
#define HEADLESS_LIB_UTIL_EMBEDDED_FILE_H_

#include <cstdint>
#include <cstdlib>

namespace headless {
namespace util {

struct EmbeddedFile {
  size_t length;
  const uint8_t* contents;
};

}  // namespace util
}  // namespace headless

#endif  // HEADLESS_LIB_UTIL_EMBEDDED_FILE_H_
