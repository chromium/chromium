// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/network_context_util.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

using NetworkContextUtilTest = PlatformTest;

// Tests that GetNetworkContextFromProfile yields the profile's NetworkContext
// while the profile is alive and nullptr once it is destroyed.
TEST_F(NetworkContextUtilTest, FollowsProfileLifetime) {
  web::WebTaskEnvironment task_environment;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  network::NetworkContextGetter getter =
      base::BindRepeating(&GetNetworkContextFromProfile, profile->AsWeakPtr());

  ASSERT_NE(getter.Run(), nullptr);
  EXPECT_EQ(getter.Run(), profile->GetNetworkContext());

  profile.reset();
  EXPECT_EQ(getter.Run(), nullptr);
}

// Tests that GetNetworkContextFromProfile returns nullptr for a null WeakPtr.
TEST_F(NetworkContextUtilTest, NullWeakPtrReturnsNull) {
  EXPECT_EQ(GetNetworkContextFromProfile(base::WeakPtr<ProfileIOS>()), nullptr);
}

}  // namespace
