// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google/model/google_brand.h"

namespace ios {
namespace google_brand {

bool IsOrganic(const std::string& brand) {
  // An empty brand string on iOS is used for organic installation. All other
  // iOS brand string are non-organic.
  return brand.empty();
}

}  // namespace google_brand
}  // namespace ios
