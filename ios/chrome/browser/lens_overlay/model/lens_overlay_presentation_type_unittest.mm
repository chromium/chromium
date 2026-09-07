// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_presentation_type.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

// A fake UITraitEnvironment for testing size class trait collections.
@interface FakeTraitEnvironment : NSObject <UITraitEnvironment>
- (instancetype)initWithTraitCollection:(UITraitCollection*)traitCollection;
@end

@implementation FakeTraitEnvironment {
  UITraitCollection* _traitCollection;
}

- (instancetype)initWithTraitCollection:(UITraitCollection*)traitCollection {
  self = [super init];
  if (self) {
    _traitCollection = traitCollection;
  }
  return self;
}

- (UITraitCollection*)traitCollection {
  return _traitCollection;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
}
@end

namespace {

FakeTraitEnvironment* CreateRegularXRegularEnvironment() {
  UITraitCollection* traits = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
        mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
        mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
      }];
  return [[FakeTraitEnvironment alloc] initWithTraitCollection:traits];
}

FakeTraitEnvironment* CreateCompactEnvironment() {
  UITraitCollection* traits = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
        mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
        mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
      }];
  return [[FakeTraitEnvironment alloc] initWithTraitCollection:traits];
}

using LensOverlayPresentationTypeTest = PlatformTest;

// Test that Lens Overlay (non-LVF) uses side panel on regular x regular.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_NonLVF_RegularXRegular) {
  FakeTraitEnvironment* env = CreateRegularXRegularEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(env, /*is_lvf=*/false),
            lens::ResultPagePresentationType::kSidePanel);
}

// Test that Lens Overlay (non-LVF) uses bottom sheet on compact environment.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_NonLVF_Compact) {
  FakeTraitEnvironment* env = CreateCompactEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(env, /*is_lvf=*/false),
            lens::ResultPagePresentationType::kEdgeAttachedBottomSheet);
}

// Test that Lens Overlay (non-LVF) is unaffected when kEnableLensOnIPad is
// enabled.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_NonLVF_UnaffectedByFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnableLensOnIPad, {{kEnableLensOnIPadPresentationStyleParam,
                           kEnableLensOnIPadPresentationStyleBottomSheet}});

  FakeTraitEnvironment* env = CreateRegularXRegularEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(env, /*is_lvf=*/false),
            lens::ResultPagePresentationType::kSidePanel);
}

// Test that on phone, LVF always uses bottom sheet.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_Phone_LVF) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    GTEST_SKIP() << "Test requires phone form factor.";
  }

  FakeTraitEnvironment* env = CreateCompactEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(env, /*is_lvf=*/true),
            lens::ResultPagePresentationType::kEdgeAttachedBottomSheet);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableLensOnIPad);
  EXPECT_EQ(lens::ResultPagePresentationFor(env, /*is_lvf=*/true),
            lens::ResultPagePresentationType::kEdgeAttachedBottomSheet);
}

// Test that on tablet, LVF falls back to regular behavior when
// kEnableLensOnIPad is disabled.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_Tablet_LVF_FeatureDisabled) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    GTEST_SKIP() << "Test requires tablet form factor.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableLensOnIPad);

  FakeTraitEnvironment* regularEnv = CreateRegularXRegularEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(regularEnv, /*is_lvf=*/true),
            lens::ResultPagePresentationType::kSidePanel);

  FakeTraitEnvironment* compactEnv = CreateCompactEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(compactEnv, /*is_lvf=*/true),
            lens::ResultPagePresentationType::kEdgeAttachedBottomSheet);
}

// Test that on tablet, LVF uses bottom sheet by default when
// kEnableLensOnIPad is enabled.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_Tablet_LVF_FeatureEnabledDefault) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    GTEST_SKIP() << "Test requires tablet form factor.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableLensOnIPad);

  FakeTraitEnvironment* regularEnv = CreateRegularXRegularEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(regularEnv, /*is_lvf=*/true),
            lens::ResultPagePresentationType::kEdgeAttachedBottomSheet);
}

// Test that on tablet, LVF uses bottom sheet when kEnableLensOnIPad is
// enabled with bottom sheet param.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_Tablet_LVF_FeatureEnabledBottomSheet) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    GTEST_SKIP() << "Test requires tablet form factor.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnableLensOnIPad, {{kEnableLensOnIPadPresentationStyleParam,
                           kEnableLensOnIPadPresentationStyleBottomSheet}});

  FakeTraitEnvironment* regularEnv = CreateRegularXRegularEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(regularEnv, /*is_lvf=*/true),
            lens::ResultPagePresentationType::kEdgeAttachedBottomSheet);
}

// Test that on tablet, LVF uses side panel when kEnableLensOnIPad is enabled
// with side panel param.
TEST_F(LensOverlayPresentationTypeTest,
       TestResultPagePresentationFor_Tablet_LVF_FeatureEnabledSidePanel) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    GTEST_SKIP() << "Test requires tablet form factor.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnableLensOnIPad, {{kEnableLensOnIPadPresentationStyleParam,
                           kEnableLensOnIPadPresentationStyleSidePanel}});

  FakeTraitEnvironment* regularEnv = CreateRegularXRegularEnvironment();
  EXPECT_EQ(lens::ResultPagePresentationFor(regularEnv, /*is_lvf=*/true),
            lens::ResultPagePresentationType::kSidePanel);
}

}  // namespace
