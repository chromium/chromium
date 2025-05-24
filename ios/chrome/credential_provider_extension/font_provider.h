// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FONT_PROVIDER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FONT_PROVIDER_H_

#import <UIKit/UIKit.h>

namespace ios {
namespace provider {

// Returns a regular-weight font for use when displaying branded product names,
// or a system default where these fonts aren't bundled.
UIFont* GetBrandedProductRegularFont(CGFloat size);

}  // namespace provider
}  // namespace ios

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FONT_PROVIDER_H_
