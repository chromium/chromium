// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_NETWORK_CONTEXT_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_NETWORK_CONTEXT_UTIL_H_

#include "base/memory/weak_ptr.h"

class ProfileIOS;

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

// Returns the NetworkContext from `weak_profile` or null if the profile has
// been deallocated. Can be used to bind callbacks.
network::mojom::NetworkContext* GetNetworkContextFromProfile(
    base::WeakPtr<ProfileIOS> weak_profile);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_NETWORK_CONTEXT_UTIL_H_
