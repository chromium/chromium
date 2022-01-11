// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_URL_UTILS_H_
#define CONTENT_COMMON_URL_UTILS_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT url::Origin OriginFromAndroidPackageName(
    const std::string& package_name);
#endif

CONTENT_EXPORT bool IsAndroidAppOrigin(
    const absl::optional<url::Origin>& origin);

}  // namespace content

#endif  // CONTENT_COMMON_URL_UTILS_H_
