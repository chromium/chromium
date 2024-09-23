// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ios/public/provider/chrome/browser/omaha/omaha_api.h"

namespace ios {
namespace provider {
namespace {

const char kTestUpdateServerURL[] = "https://iosupdatetest.chromium.org";

const char kTestApplicationID[] = "{TestApplicationID}";

// Brand-codes are composed of four capital letters.
const char kTestBrandCode[] = "RIMZ";

}  // anonymous namespace

GURL GetOmahaUpdateServerURL() {
  return GURL(kTestUpdateServerURL);
}

std::string GetOmahaApplicationId() {
  return kTestApplicationID;
}

void SetOmahaExtraAttributes(std::string_view element, AttributeSetter setter) {
  if (element == "app") {
    setter.Run("brand", kTestBrandCode);
    setter.Run("appid", kTestApplicationID);
  }
}

}  // namespace provider
}  // namespace ios
