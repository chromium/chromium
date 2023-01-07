// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ALERT_VIEW_ALERT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_ALERT_VIEW_ALERT_CONSUMER_H_

#import <Foundation/Foundation.h>

@class AlertAction;
@class TextFieldConfiguration;

// AlertConsumer defines methods to set the contents of an alert.
@protocol AlertConsumer <NSObject>

// Sets the title of the alert.
- (void)setTitle:(NSString*)title;

// Sets the message of the alert.
- (void)setMessage:(NSString*)message;

// Sets the text field configurations for this alert. One text field will be
// created for each `TextFieldConfiguration`.
- (void)setTextFieldConfigurations:
    (NSArray<TextFieldConfiguration*>*)textFieldConfigurations;

// Sets the actions for this alert.
- (void)setActions:(NSArray<AlertAction*>*)actions;

// Sets the accessibility identifier for the alert view.
- (void)setAlertAccessibilityIdentifier:(NSString*)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_ALERT_VIEW_ALERT_CONSUMER_H_
