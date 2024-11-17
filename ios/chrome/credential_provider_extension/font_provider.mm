// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/font_provider.h"

namespace ios {
namespace provider {

UIFont* GetBrandedProductRegularFont(CGFloat size) {
  // Chromium does not have branded fonts, so return system default instead.
  return [UIFont systemFontOfSize:size];
}

}  // namespace provider
}  // namespace ios
