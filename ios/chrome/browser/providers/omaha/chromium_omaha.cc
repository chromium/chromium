// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ios/public/provider/chrome/browser/omaha/omaha_api.h"

namespace ios {
namespace provider {

GURL GetOmahaUpdateServerURL() {
  // Chromium does not uses Omaha.
  return GURL();
}

std::string GetOmahaApplicationId() {
  // Chromium does not uses Omaha.
  return std::string();
}

void SetOmahaExtraAttributes(std::string_view element, AttributeSetter setter) {
  // Chromium does not uses Omaha.
}

}  // namespace provider
}  // namespace ios
