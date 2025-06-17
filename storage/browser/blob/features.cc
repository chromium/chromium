// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/features.h"

#include "build/buildflag.h"

namespace features {

// Please keep features in alphabetical order.
BASE_FEATURE(kBlockCrossPartitionBlobUrlFetching,
             "BlockCrossPartitionBlobUrlFetching",
// TODO(crbug.com/421810301): Temporarily disable this feature on ChromeOS due
// to a regression.
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Please keep features in alphabetical order.

}  // namespace features
