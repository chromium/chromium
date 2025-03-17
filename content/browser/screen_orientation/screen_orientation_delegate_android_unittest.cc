// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"

namespace content {

namespace {

// Override the delegate so that we can lie about our phone-ness.
class DelegateForTesting : public ScreenOrientationDelegateAndroid {
 public:
  void set_is_phone(bool is_phone) { is_phone_ = is_phone; }

 protected:
  bool IsPhone() const override { return is_phone_; }

 private:
  bool is_phone_ = true;
};

}  // namespace

class ScreenOrientationDelegateAndroidTest
    : public RenderViewHostImplTestHarness {
 public:
  ScreenOrientationDelegateAndroidTest() = default;

  void SetUp() override {
    content::RenderViewHostImplTestHarness::SetUp();

    delegate_ = std::make_unique<DelegateForTesting>();
  }

  DelegateForTesting* delegate() const { return delegate_.get(); }

 private:
  std::unique_ptr<DelegateForTesting> delegate_;
};

TEST_F(ScreenOrientationDelegateAndroidTest, RestrictedToPhoneWorks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kRestrictOrientationLockToPhones},
                                {});

  // Should only be supported for phones.
  delegate()->set_is_phone(true);
  EXPECT_TRUE(delegate()->ScreenOrientationProviderSupported(contents()));
  delegate()->set_is_phone(false);
  EXPECT_FALSE(delegate()->ScreenOrientationProviderSupported(contents()));
}

TEST_F(ScreenOrientationDelegateAndroidTest, NotRestrictedToPhoneWorks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {features::kRestrictOrientationLockToPhones});

  // Should be supported for both phones and non-phones.
  delegate()->set_is_phone(true);
  EXPECT_TRUE(delegate()->ScreenOrientationProviderSupported(contents()));
  delegate()->set_is_phone(false);
  EXPECT_TRUE(delegate()->ScreenOrientationProviderSupported(contents()));
}

}  // namespace content
