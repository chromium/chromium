// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_TEST_FAKE_ALERT_CONSUMER_H_
#define IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_TEST_FAKE_ALERT_CONSUMER_H_

#import "ios/chrome/browser/alert_view/ui_bundled/alert_consumer.h"

// Fake version of AlertConsumer that allows read access to consumed values.
@interface FakeAlertConsumer : NSObject <AlertConsumer>

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* message;
@property(nonatomic, copy)
    NSArray<TextFieldConfiguration*>* textFieldConfigurations;
@property(nonatomic, copy) NSArray<NSArray<AlertAction*>*>* actions;
@property(nonatomic, copy) NSString* alertAccessibilityIdentifier;
@property(nonatomic, assign) BOOL shouldShowActivityIndicator;
@property(nonatomic, assign) BOOL actionButtonsAreInitiallyDisabled;

@end

#endif  // IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_TEST_FAKE_ALERT_CONSUMER_H_
