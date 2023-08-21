// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/coordinators/features.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator+internal.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "testing/platform_test.h"

namespace {
class InfobarCoordinatorTest : public PlatformTest {
 public:
  InfobarCoordinatorTest();
  ~InfobarCoordinatorTest() override;
};

InfobarCoordinatorTest::InfobarCoordinatorTest() {}
InfobarCoordinatorTest::~InfobarCoordinatorTest() {}

// Tests the duration lenght of the translate infobar.
TEST_F(InfobarCoordinatorTest, TranslateInfobarPresentationDuration) {
  InfobarCoordinator* coordinator = [[InfobarCoordinator alloc]
      initWithInfoBarDelegate:nil
                 badgeSupport:YES
                         type:InfobarType::kInfobarTypeTranslate];

  ASSERT_EQ(kInfobarBannerDefaultPresentationDuration,
            [coordinator infobarPresentationDuration]);
}

// Tests the duration lenght of the password infobar, when overriden by a
// feature param.
TEST_F(InfobarCoordinatorTest, PasswordInfobarPresentationDuration) {
  InfobarCoordinator* coordinator = [[InfobarCoordinator alloc]
      initWithInfoBarDelegate:nil
                 badgeSupport:YES
                         type:InfobarType::kInfobarTypePasswordSave];

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPasswordInfobarDisplayLength, {{"duration-seconds", "20"}});

  ASSERT_EQ(base::Seconds(20), [coordinator infobarPresentationDuration]);
}

}  // namespace
