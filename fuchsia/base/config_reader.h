// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_CONFIG_READER_H_
#define FUCHSIA_BASE_CONFIG_READER_H_

#include "base/values.h"

namespace cr_fuchsia {

// Loads and parses configuration data from the environment.
// Returns a null value if the file(s) do not exist.
// CHECK-fails if the file(s) are present but not parseable.
const absl::optional<base::Value>& LoadPackageConfig();

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_CONFIG_READER_H_
