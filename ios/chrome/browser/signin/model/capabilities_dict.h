// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CAPABILITIES_DICT_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CAPABILITIES_DICT_H_

#import <Foundation/Foundation.h>

namespace ios {

// Dictionary from capability name, as in `account_capabilities.cc` to a
// `ChromeIdentityCapabilityResult` encoded as a NSNumber.
using CapabilitiesDict = NSDictionary<NSString*, NSNumber*>;

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CAPABILITIES_DICT_H_
