// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/android_overlay_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class AndroidOverlayProviderImplTest : public ::testing::Test {};

TEST_F(AndroidOverlayProviderImplTest, AreOverlaysSupported) {
  // Right now, AndroidOverlay always claims to support overlays.
  AndroidOverlayProvider* provider = AndroidOverlayProviderImpl::GetInstance();
  ASSERT_NE(provider, nullptr);
  ASSERT_TRUE(provider->AreOverlaysSupported());
}

}  // namespace
}  // namespace content
