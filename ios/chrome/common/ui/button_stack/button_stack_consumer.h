// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONSUMER_H_
#define IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONSUMER_H_

@class ButtonStackConfiguration;

// Protocol for a consumer of ButtonStack state.
@protocol ButtonStackConsumer

// Updates the loading state of the button stack.
- (void)setLoading:(BOOL)loading;

// Updates the confirmed state of the button stack.
- (void)setConfirmed:(BOOL)confirmed;

// Reloads the configuration of the button stack.
- (void)reloadConfiguration;

// Updates and reloads the configuration of the button stack.
- (void)updateConfiguration:(ButtonStackConfiguration*)configuration;

@end

#endif  // IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_CONSUMER_H_
