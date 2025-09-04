// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_utils.h"

#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

ProfileIOS* GetContextToUseForKeyedServiceFactory(
    ProfileIOS* profile,
    ProfileSelection profile_selection) {
  if (!profile) {
    return nullptr;
  }

  if (!profile->IsOffTheRecord()) {
    return profile;
  }

  switch (profile_selection) {
    case ProfileSelection::kNoInstanceInIncognito:
      return nullptr;

    case ProfileSelection::kRedirectedInIncognito:
      return profile->GetOriginalProfile();

    case ProfileSelection::kOwnInstanceInIncognito:
      return profile;
  }
}
