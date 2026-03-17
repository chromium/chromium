// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_FAKE_REAUTHENTICATION_SERVICE_UTIL_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_FAKE_REAUTHENTICATION_SERVICE_UTIL_H_

#import <memory>

#import "components/keyed_service/core/keyed_service.h"

class ProfileIOS;

// Returns a reauthentication service with a fake reauthentication module.
std::unique_ptr<KeyedService> CreateFakeReauthService(ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_FAKE_REAUTHENTICATION_SERVICE_UTIL_H_
