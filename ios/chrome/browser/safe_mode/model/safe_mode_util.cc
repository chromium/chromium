// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_mode/model/safe_mode_util.h"

#include <mach-o/dyld.h>
#include <stdint.h>

namespace safe_mode_util {

std::vector<std::string> GetLoadedImages(const char* path_filter) {
  std::vector<std::string> images;
  uint32_t image_count = _dyld_image_count();
  for (uint32_t i = 0; i < image_count; ++i) {
    const char* path = _dyld_get_image_name(i);
    if (path_filter && strncmp(path, path_filter, strlen(path_filter)) != 0) {
      continue;
    }
    images.push_back(path);
  }
  return images;
}

}  // namespace safe_mode_util
