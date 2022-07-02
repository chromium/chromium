// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/util_swift.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#include "base/test/ios/wait_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Sets up layout guide center.
class LayoutGuideCenterTest : public PlatformTest {
 protected:
  LayoutGuideCenterTest() : center_([[LayoutGuideCenter alloc] init]) {}

  LayoutGuideCenter* center_;
};

// Checks that a tracking layout guide is correctly updated to match the
// reference view's frame.
TEST_F(LayoutGuideCenterTest, LayoutGuideMatchesReferenceView) {
  CGRect rect = CGRectMake(10, 20, 30, 40);
  UIView* reference_view = [[UIView alloc] initWithFrame:rect];
  [center_ referenceView:reference_view underName:@"view"];
  // Set up the tracking layout guide.
  UILayoutGuide* layout_guide = [center_ makeLayoutGuideNamed:@"view"];
  UIView* view = [[UIView alloc] init];
  [view addLayoutGuide:layout_guide];

  UIWindow* window = [[UIWindow alloc] init];
  [window addSubview:reference_view];
  [window addSubview:view];

  EXPECT_TRUE(CGRectEqualToRect(layout_guide.layoutFrame, rect));
}

// Checks that a tracking layout guide is correctly updated to track the
// reference view's frame.
TEST_F(LayoutGuideCenterTest, LayoutGuideTracksReferenceView) {
  UIView* reference_view = [[UIView alloc] init];
  [center_ referenceView:reference_view underName:@"view"];
  // Set up the tracking layout guide.
  UILayoutGuide* layout_guide = [center_ makeLayoutGuideNamed:@"view"];
  UIView* view = [[UIView alloc] init];
  [view addLayoutGuide:layout_guide];
  UIWindow* window = [[UIWindow alloc] init];
  [window addSubview:reference_view];
  [window addSubview:view];
  EXPECT_TRUE(CGRectEqualToRect(layout_guide.layoutFrame, CGRectZero));

  for (NSValue* rectValue in @[
         @(CGRectMake(0, -10, 20, 30)), @(CGRectMake(3, 14, 15, 93)),
         @(CGRectZero)
       ]) {
    CGRect rect = rectValue.CGRectValue;
    reference_view.frame = rect;
    [window setNeedsLayout];
    [window layoutIfNeeded];

    // Wait until the frame is updated.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      return CGRectEqualToRect(layout_guide.layoutFrame, rect);
    }));
  }
}

// Checks that a tracking layout guide is correctly updated to match the
// reference view's frame in a different window.
TEST_F(LayoutGuideCenterTest,
       LayoutGuideMatchesReferenceViewInDifferentWindow) {
  CGRect rect = CGRectMake(10, 20, 30, 40);
  UIView* reference_view = [[UIView alloc] initWithFrame:rect];
  [center_ referenceView:reference_view underName:@"view"];
  // Set up the tracking layout guide.
  UILayoutGuide* layout_guide = [center_ makeLayoutGuideNamed:@"view"];
  UIView* view = [[UIView alloc] init];
  [view addLayoutGuide:layout_guide];
  // Set up windows in the same scene.
  UIWindowScene* scene = base::mac::ObjCCastStrict<UIWindowScene>(
      [UIApplication.sharedApplication.connectedScenes anyObject]);
  UIWindow* reference_window = [[UIWindow alloc] init];
  reference_window.windowScene = scene;
  UIWindow* window = [[UIWindow alloc] init];
  window.windowScene = scene;

  [reference_window addSubview:reference_view];
  [window addSubview:view];

  EXPECT_TRUE(CGRectEqualToRect(layout_guide.layoutFrame, rect));
}

// Checks that LayoutGuideCenter only keeps weak references to referenced views.
TEST_F(LayoutGuideCenterTest, WeakReferenceView) {
  UIView* reference_view = [[UIView alloc] init];
  // Create a weak reference to the view, to check it gets nilled out.
  __weak UIView* weak_reference_view = reference_view;

  @autoreleasepool {
    [center_ referenceView:reference_view underName:@"view"];
    reference_view = nil;
  }

  // The center should not have kept the view around.
  EXPECT_EQ(weak_reference_view, nil);
}

// Checks that LayoutGuideCenter only keeps weak references to layout guides.
// Warning: this test may fail while debugging, because of intricacies of
// NSHashTable. See `HashTableWeakReference` below for more context.
TEST_F(LayoutGuideCenterTest, WeakLayoutGuide) {
  UILayoutGuide* layout_guide = [center_ makeLayoutGuideNamed:@"view"];
  EXPECT_NE(layout_guide, nil);
  // Create a weak reference to the layout guide, to check it gets nilled out.
  __weak UILayoutGuide* weak_layout_guide = layout_guide;

  @autoreleasepool {
    layout_guide = nil;
  }

  // This can fail if you are breaking in the debugger and inspecting the hash
  // table. This is due to NSHashTable not providing any guarantee as for when
  // the elements are released.
  EXPECT_EQ(weak_layout_guide, nil);
}

// Checks that NSHashTable references its content weakly.
// Warning: NSHashTable's behavior is not really deterministic (from a client
// perspective), in that it decides when it's removing the weak references. When
// breaking in the debugger in this test and poking around the hash table, the
// test fails, for example.
TEST_F(LayoutGuideCenterTest, HashTableWeakReference) {
  UILayoutGuide* layout_guide = [[UILayoutGuide alloc] init];
  __weak UILayoutGuide* weak_layout_guide = layout_guide;
  NSHashTable<UILayoutGuide*>* hash_table = [NSHashTable weakObjectsHashTable];
  [hash_table addObject:layout_guide];

  @autoreleasepool {
    layout_guide = nil;
  }

  // This can fail if you are breaking in the debugger and inspecting the hash
  // table. This is due to NSHashTable not providing any guarantee as for when
  // the elements are released.
  EXPECT_EQ(weak_layout_guide, nil);
}
