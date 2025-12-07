// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_util.h"

#import <UIKit/UIKit.h>

#import <string_view>

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"

namespace {
// Unique identifier used by device that do not support multiple scenes.
constexpr std::string_view kSyntheticSessionIdentifier =
    "{SyntheticIdentifier}";
}  // namespace

std::string SessionIdentifierForScene(UIScene* scene) {
  if (base::ios::IsMultipleScenesSupported()) {
    std::string identifier =
        base::SysNSStringToUTF8([[scene session] persistentIdentifier]);

    DCHECK_NE(identifier, "");
    DCHECK_NE(identifier, kSyntheticSessionIdentifier);
    return identifier;
  }

  return std::string(kSyntheticSessionIdentifier);
}
