// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FONT_FONT_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FONT_FONT_API_H_

#import <UIKit/UIKit.h>

namespace ios {
namespace provider {

// Returns a regular-weight font for use when displaying branded product names,
// or a system default where these fonts aren't bundled.
UIFont* GetBrandedProductRegularFont(CGFloat size);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FONT_FONT_API_H_
