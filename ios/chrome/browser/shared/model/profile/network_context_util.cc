// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/network_context_util.h"

#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

network::mojom::NetworkContext* GetNetworkContextFromProfile(
    base::WeakPtr<ProfileIOS> weak_profile) {
  if (ProfileIOS* profile = weak_profile.get()) {
    return profile->GetNetworkContext();
  }
  return nullptr;
}
