// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_H_
#define IOS_CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_H_

#include <string>

namespace ios {
namespace google_brand {

// Returns in |brand| the brand code or distribution tag that has been assigned
// to a partner. Returns false if the information is not available.
bool GetBrand(std::string* brand);

// True if a build is strictly organic, according to its brand code.
bool IsOrganic(const std::string& brand);

}  // namespace google_brand
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_H_
