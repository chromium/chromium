// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_API_H_

#include <memory>

#include "ios/chrome/browser/follow/model/follow_configuration.h"
#include "ios/chrome/browser/follow/model/follow_service.h"

namespace ios {
namespace provider {

// Creates a new instance of FollowService.
std::unique_ptr<FollowService> CreateFollowService(
    FollowConfiguration* configuration);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_API_H_
