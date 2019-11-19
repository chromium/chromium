// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_toolbar.h"
#include "base/logging.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A class that counts the number of times the |dummyMethod:| method is called.
@interface DummyObserver : NSObject

// The number of times |dummyMethod:| is invoked.
@property(nonatomic, readonly) int dummyMethodCallCount;

// The method whose invocation increases |dummyMethodCallCount| by one.
- (void)dummyMethod:(id)parameter;

@end

@implementation DummyObserver

@synthesize dummyMethodCallCount = _dummyMethodCallCount;

- (void)dummyMethod:(id)parameter {
  _dummyMethodCallCount++;
}

@end

namespace {

class OpenInToolbarTest : public PlatformTest {
 protected:
  UIButton* GetOpenInButtonInToolBar(OpenInToolbar* toolbar) {
    NSArray* subviews = [toolbar subviews];
    // Assumes there is only one UIButton in the toolbar.
    for (UIView* subview in subviews) {
      if ([subview isKindOfClass:[UIButton class]]) {
        return (UIButton*)subview;
      }
    }
    return nil;
  }
};

TEST_F(OpenInToolbarTest, TestButtonActionAndSelector) {
  DummyObserver* dummyObserver = [[DummyObserver alloc] init];
  OpenInToolbar* openInToolbar =
      [[OpenInToolbar alloc] initWithTarget:dummyObserver
                                     action:@selector(dummyMethod:)];
  UIButton* button = GetOpenInButtonInToolBar(openInToolbar);
  ASSERT_TRUE(button);
  EXPECT_EQ([dummyObserver dummyMethodCallCount], 0);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ([dummyObserver dummyMethodCallCount], 1);
}

}  // namespace
