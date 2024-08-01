// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_LOCATION_BAR_OFFSET_PROVIDER_H_
#define IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_LOCATION_BAR_OFFSET_PROVIDER_H_

#import <Foundation/Foundation.h>

// Protocol for vending an x offset for a string.
@protocol LocationBarOffsetProvider<NSObject>

// Provides an offset for a given string in the callee's coordinates, if any.
// Returns a default value (based on callee's font) if the `string` is not a
// substring of the string displayed by callee.
- (CGFloat)xOffsetForString:(NSString*)string;

@end

#endif  // IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_LOCATION_BAR_OFFSET_PROVIDER_H_
