// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol SystemIdentity;

@protocol AccountPickerConsumer <NSObject>

// Starts the spinner and disables buttons.
- (void)startValidationSpinner;

// Stops the spinner and enables buttons.
- (void)stopValidationSpinner;

// Shows/hides the identity button in the account confirmation screen.
- (void)setIdentityButtonHidden:(BOOL)hidden animated:(BOOL)animated;

// Set the currently selected identity.
- (void)setSelectedIdentity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONSUMER_H_
