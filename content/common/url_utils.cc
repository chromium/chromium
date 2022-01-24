// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_utils.h"

#include <string>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "content/common/url_schemes.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace content {

#if defined(OS_ANDROID)
url::Origin OriginFromAndroidPackageName(const std::string& package_name) {
  return url::Origin::Create(
      GURL(base::StrCat({kAndroidAppScheme, ":", package_name})));
}
#endif

bool IsAndroidAppOrigin(const absl::optional<url::Origin>& origin) {
#if defined(OS_ANDROID)
  return origin && origin->scheme() == kAndroidAppScheme;
#else
  return false;
#endif
}

}  // namespace content
