// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_SSO_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_SSO_API_H_

#import <Foundation/Foundation.h>

// Abstract protocol of the Single-Sign-On Service used by Chrome.
@protocol SingleSignOnService <NSObject>
@end

namespace ios {
namespace provider {

// Creates a new instance of SingleSignOnService.
id<SingleSignOnService> CreateSSOService();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_SSO_API_H_
