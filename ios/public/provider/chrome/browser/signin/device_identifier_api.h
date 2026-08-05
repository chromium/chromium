// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_DEVICE_IDENTIFIER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_DEVICE_IDENTIFIER_API_H_

#import <string>

namespace ios::provider {

// Returns a unique device identifier.
std::string GetDeviceIdentifier();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_DEVICE_IDENTIFIER_API_H_
