// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/util_swift.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Calls a block when receiving a KVO notification.
@interface Observer : NSObject
@property(nonatomic, copy) void (^onChange)(id object, NSDictionary* change);
@end

@implementation Observer

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (self.onChange) {
    self.onChange(object, change);
  }
}

@end

// Sets up a window, a view, and an observer on the view's window property.
class UIViewWindowObservingTest : public PlatformTest {
 protected:
  UIViewWindowObservingTest()
      : window_([[UIWindow alloc] init]),
        view_([[UIView alloc] init]),
        observer_([[Observer alloc] init]) {
    [view_
        addObserver:observer_
         forKeyPath:@"window"
            options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld
            context:nullptr];
  }

  ~UIViewWindowObservingTest() override {
    [view_ removeObserver:observer_ forKeyPath:@"window"];
  }

  UIWindow* window_;
  UIView* view_;
  // Observes the view's `window` property.
  Observer* observer_;
};

// Checks that the observer is not called when the view is added to the window
// and `cr_supportsWindowObserving` is set to `NO`.
TEST_F(UIViewWindowObservingTest, WindowObservingUnset) {
  UIView.cr_supportsWindowObserving = NO;
  EXPECT_FALSE(UIView.cr_supportsWindowObserving);
  observer_.onChange = ^(id object, id change) {
    FAIL() << "KVO should not have triggered before being enabled.";
  };

  [window_ addSubview:view_];
}

// Checks that the observer is called with the correct old and new windows when
// the view is added to the window and `cr_supportsWindowObserving` is set.
TEST_F(UIViewWindowObservingTest, WindowObservingSet) {
  UIView.cr_supportsWindowObserving = YES;
  EXPECT_TRUE(UIView.cr_supportsWindowObserving);
  __block BOOL callback_called = NO;
  observer_.onChange = ^(id object, id change) {
    callback_called = YES;
    EXPECT_EQ(object, view_);
    EXPECT_EQ(change[NSKeyValueChangeOldKey], [NSNull null]);
    EXPECT_EQ(change[NSKeyValueChangeNewKey], window_);
  };

  [window_ addSubview:view_];

  EXPECT_TRUE(callback_called);
}

// Checks that the observer is called with the correct old and new windows when
// a textfield is added to the window and `cr_supportsWindowObserving` is set.
TEST_F(UIViewWindowObservingTest, TextField_WindowObservingSet) {
  UIView.cr_supportsWindowObserving = YES;
  EXPECT_TRUE(UIView.cr_supportsWindowObserving);
  UITextField* textField = [[UITextField alloc] init];
  Observer* observer = [[Observer alloc] init];
  [textField
      addObserver:observer
       forKeyPath:@"window"
          options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld
          context:nullptr];
  __block BOOL callback_called = NO;
  observer.onChange = ^(id object, id change) {
    callback_called = YES;
    EXPECT_EQ(object, textField);
    EXPECT_EQ(change[NSKeyValueChangeOldKey], [NSNull null]);
    EXPECT_EQ(change[NSKeyValueChangeNewKey], window_);
  };

  [window_ addSubview:textField];

  EXPECT_TRUE(callback_called);

  // Clean up.
  [textField removeObserver:observer forKeyPath:@"window"];
}

// Checks that the observer is not called when the view is added to the window
// and `cr_supportsWindowObserving` is reset to `NO`.
TEST_F(UIViewWindowObservingTest, WindowObservingReset) {
  UIView.cr_supportsWindowObserving = YES;
  UIView.cr_supportsWindowObserving = NO;
  observer_.onChange = ^(id object, id change) {
    FAIL() << "KVO should not have triggered after being disabled.";
  };

  [window_ addSubview:view_];
}
