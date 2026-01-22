// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MUTATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MUTATOR_H_

#import <UIKit/UIKit.h>

// Mutator for the Location Bar.
@protocol LocationBarMutator <NSObject>

// Loads the given `query`.
- (void)loadQuery:(NSString*)query;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MUTATOR_H_
