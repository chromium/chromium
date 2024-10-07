// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SHARE_KIT_SHARE_KIT_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SHARE_KIT_SHARE_KIT_API_H_

#include <memory>

class ShareKitService;
struct ShareKitServiceConfiguration;

namespace ios::provider {

// Creates a new instance of ShareKitService.
std::unique_ptr<ShareKitService> CreateShareKitService(
    const ShareKitServiceConfiguration& configuration);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SHARE_KIT_SHARE_KIT_API_H_
