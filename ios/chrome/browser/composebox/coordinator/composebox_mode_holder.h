// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_MODE_HOLDER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_MODE_HOLDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"

// Observer protocol for ComposeboxModeHolder.
@protocol ComposeboxModeObserver

// Called when the composebox mode changes.
- (void)composeboxModeDidChange:(ComposeboxMode)mode;

@end

// Holds and allows observation of the composebox mode.
@interface ComposeboxModeHolder : NSObject

@property(nonatomic, assign) ComposeboxMode mode;

- (void)addObserver:(id<ComposeboxModeObserver>)observer;
- (void)removeObserver:(id<ComposeboxModeObserver>)observer;

// YES if the current mode is kRegularSearch.
- (BOOL)isRegularSearch;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_MODE_HOLDER_H_
