// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace crypto {

#if BUILDFLAG(IS_MAC)
// Not yet enabled by default.
BASE_FEATURE(kEnableMacUnexportableKeys,
             "EnableMacUnexportableKeys",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace crypto
