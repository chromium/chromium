// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_UTILS_H_

#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

// Returns the context to use for KeyedServiceFactory according for `profile`
// according to `profile_selection`.
ProfileIOS* GetContextToUseForKeyedServiceFactory(
    ProfileIOS* profile,
    ProfileSelection profile_selection);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_UTILS_H_
