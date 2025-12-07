// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_OMNIBOX_STATE_PROVIDER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_OMNIBOX_STATE_PROVIDER_H_

#import <Foundation/Foundation.h>

/// Provides the omnibox state.
@protocol OmniboxStateProvider <NSObject>

/// Returns whether the omnibox is focused.
- (BOOL)isOmniboxFocused;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_OMNIBOX_STATE_PROVIDER_H_
