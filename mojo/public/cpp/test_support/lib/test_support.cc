// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/test_support/test_support.h"

#include <stdlib.h>

namespace mojo {
namespace test {

std::vector<std::string> EnumerateSourceRootRelativeDirectory(
    const std::string& relative_path) {
  char** names = MojoTestSupportEnumerateSourceRootRelativeDirectory(
      relative_path.c_str());
  std::vector<std::string> results;
  for (char** ptr = names; *ptr != nullptr; ++ptr) {
    results.push_back(*ptr);
    free(*ptr);
  }
  free(names);
  return results;
}

}  // namespace test
}  // namespace mojo
