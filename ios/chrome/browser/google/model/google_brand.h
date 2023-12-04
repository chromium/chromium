// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_BRAND_H_
#define IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_BRAND_H_

#include <string>

namespace ios {
namespace google_brand {

// True if a build is strictly organic, according to its brand code.
bool IsOrganic(const std::string& brand);

}  // namespace google_brand
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_BRAND_H_
