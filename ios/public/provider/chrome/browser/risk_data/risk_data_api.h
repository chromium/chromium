// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_RISK_DATA_RISK_DATA_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_RISK_DATA_RISK_DATA_API_H_

#import <Foundation/Foundation.h>

namespace ios {
namespace provider {

// Returns risk data used in Wallet requests.
NSString* GetRiskData();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_RISK_DATA_RISK_DATA_API_H_
