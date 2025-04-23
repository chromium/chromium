// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_MEDIATOR_TESTING_H_

// Testing category exposing private methods of OmniboxPopupMediator for tests.
@interface OmniboxPopupMediator (Testing)

@property(nonatomic, strong, readonly)
    NSArray<id<AutocompleteSuggestionGroup>>* suggestionGroups;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_MEDIATOR_TESTING_H_
