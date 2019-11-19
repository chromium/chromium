// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_test_util.h"
#include "base/files/file_util.h"

namespace extensions {
namespace file_test_util {

bool WriteFile(const base::FilePath& path, base::StringPiece content) {
  return base::WriteFile(path, content.data(), content.size()) ==
         static_cast<int>(content.size());
}

}  // namespace file_test_util
}  // namespace extensions
