// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_RESOURCES_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_RESOURCES_API_H_

#import <UIKit/UIKit.h>

namespace ios {
namespace provider {

// Returns a default avatar to use when an identity is missing one.
UIImage* GetSigninDefaultAvatar();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_RESOURCES_API_H_
