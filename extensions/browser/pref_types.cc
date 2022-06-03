// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_types.h"

namespace extensions {
// Records the number of corrupted extensions that have been disabled.
const PrefMap kCorruptedDisableCount = {"extensions.corrupted_disable_count",
                                        PrefType::kInteger,
                                        PrefScope::kProfile};
}  // namespace extensions
