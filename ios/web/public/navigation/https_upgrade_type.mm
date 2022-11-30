// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/https_upgrade_type.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

std::string GetHttpsUpgradeTypeDescription(HttpsUpgradeType type) {
  switch (type) {
    case HttpsUpgradeType::kNone:
      return "None";
    case HttpsUpgradeType::kHttpsOnlyMode:
      return "HttpsOnlyMode";
    case HttpsUpgradeType::kOmnibox:
      return "Omnibox";
  }
}

}  // namespace web
