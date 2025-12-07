// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_mode/model/safe_mode_util.h"

#include <mach-o/dyld.h>
#include <stdint.h>

namespace safe_mode_util {

std::vector<std::string> GetLoadedImages(std::string_view path_filter) {
  std::vector<std::string> images;
  uint32_t image_count = _dyld_image_count();
  for (uint32_t i = 0; i < image_count; ++i) {
    // SAFETY: _dyld_get_image_name(i) returns a null-terminated string.
    const std::string_view path(_dyld_get_image_name(i));
    if (!path.starts_with(path_filter)) {
      // As all strings start by the empty string, this means that
      // path_filter is non-empty and path does not start with it.
      continue;
    }
    images.push_back(std::string(path));
  }
  return images;
}

}  // namespace safe_mode_util
