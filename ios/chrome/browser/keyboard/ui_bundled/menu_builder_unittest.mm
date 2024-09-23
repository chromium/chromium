// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/keyboard/ui_bundled/menu_builder.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake implementation of UIMenuBuilder for tests.
@interface TestUIMenuBuilder : NSObject <UIMenuBuilder>

@property(nonatomic, strong) UIMenuSystem* system;

// Whether one of the mutating methods from UIMenuBuilder was called.
@property(nonatomic) BOOL wasMutated;

@end

@implementation TestUIMenuBuilder

- (UIMenu*)menuForIdentifier:(UIMenuIdentifier)identifier {
  return nil;
}

- (UIAction*)actionForIdentifier:(UIActionIdentifier)identifier {
  return nil;
}

- (UICommand*)commandForAction:(SEL)action propertyList:(id)propertyList {
  return nil;
}

- (void)replaceMenuForIdentifier:(UIMenuIdentifier)replacedIdentifier
                        withMenu:(UIMenu*)replacementMenu {
  _wasMutated = YES;
}

- (void)replaceChildrenOfMenuForIdentifier:(UIMenuIdentifier)parentIdentifier
                         fromChildrenBlock:
                             (NSArray<UIMenuElement*>*(NS_NOESCAPE ^)(
                                 NSArray<UIMenuElement*>*))childrenBlock {
  _wasMutated = YES;
}

- (void)insertSiblingMenu:(UIMenu*)siblingMenu
    beforeMenuForIdentifier:(UIMenuIdentifier)siblingIdentifier {
  _wasMutated = YES;
}

- (void)insertSiblingMenu:(UIMenu*)siblingMenu
    afterMenuForIdentifier:(UIMenuIdentifier)siblingIdentifier {
  _wasMutated = YES;
}

- (void)insertChildMenu:(UIMenu*)childMenu
    atStartOfMenuForIdentifier:(UIMenuIdentifier)parentIdentifier {
  _wasMutated = YES;
}

- (void)insertChildMenu:(UIMenu*)childMenu
    atEndOfMenuForIdentifier:(UIMenuIdentifier)parentIdentifier {
  _wasMutated = YES;
}

- (void)removeMenuForIdentifier:(UIMenuIdentifier)removedIdentifier {
  _wasMutated = YES;
}

@end

namespace {

using MenuBuilderTest = PlatformTest;

// Checks that calling a builder for the non-main menu system doesn't affect the
// builder.
TEST_F(MenuBuilderTest, NonMainSystem_NoOp) {
  TestUIMenuBuilder* nonMainBuilder = [[TestUIMenuBuilder alloc] init];
  nonMainBuilder.system = UIMenuSystem.contextSystem;
  ASSERT_FALSE(nonMainBuilder.wasMutated);

  [MenuBuilder buildMainMenuWithBuilder:nonMainBuilder];

  EXPECT_FALSE(nonMainBuilder.wasMutated);
}

// Checks that calling a builder for the non-main menu system affects the
// builder.
TEST_F(MenuBuilderTest, MainSystem_Configured) {
  TestUIMenuBuilder* mainBuilder = [[TestUIMenuBuilder alloc] init];
  mainBuilder.system = UIMenuSystem.mainSystem;
  ASSERT_FALSE(mainBuilder.wasMutated);

  [MenuBuilder buildMainMenuWithBuilder:mainBuilder];

  EXPECT_TRUE(mainBuilder.wasMutated);
}

}  // namespace
