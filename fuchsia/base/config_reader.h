// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_CONFIG_READER_H_
#define FUCHSIA_BASE_CONFIG_READER_H_

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}

namespace cr_fuchsia {

// Return a JSON dictionary read from the calling Component's config-data.
// All *.json files in the config-data directory are read, parsed, and merged
// into a single JSON dictionary value.
// Null is returned if no config-data exists for the Component.
// CHECK()s if one or more config files are malformed, or there are duplicate
// non-dictionary fields in different config files.
const absl::optional<base::Value>& LoadPackageConfig();

// Used to test the implementation of LoadPackageConfig().
absl::optional<base::Value> LoadConfigFromDirForTest(const base::FilePath& dir);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_CONFIG_READER_H_
