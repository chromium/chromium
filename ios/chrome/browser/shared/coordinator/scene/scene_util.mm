// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_util.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/ios/ios_util.h"

namespace {
// Unique identifier used by device that do not support multiple scenes.
NSString* const kSyntheticSessionIdentifier = @"{SyntheticIdentifier}";
}  // namespace

NSString* SessionIdentifierForScene(UIScene* scene) {
  if (base::ios::IsMultipleScenesSupported()) {
    NSString* identifier = [[scene session] persistentIdentifier];

    DCHECK(identifier.length != 0);
    DCHECK(![kSyntheticSessionIdentifier isEqualToString:identifier]);
    return identifier;
  }
  return kSyntheticSessionIdentifier;
}
