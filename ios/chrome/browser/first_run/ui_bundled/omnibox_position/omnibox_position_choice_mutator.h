// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MUTATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MUTATOR_H_

/// Mutator of the omnibox position choice.
@protocol OmniboxPositionChoiceMutator

/// Selects the top omnibox option.
- (void)selectTopOmnibox;
/// Selects the bottom omnibox option.
- (void)selectBottomOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MUTATOR_H_
