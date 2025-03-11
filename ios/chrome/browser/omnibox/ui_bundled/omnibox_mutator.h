// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_MUTATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_MUTATOR_H_

#import <UIKit/UIKit.h>

/// Mutator for the omnibox.
@protocol OmniboxMutator <NSObject>

/// Removes the thumbnail.
- (void)removeThumbnail;

/// Removes the additional text.
- (void)removeAdditionalText;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_MUTATOR_H_
