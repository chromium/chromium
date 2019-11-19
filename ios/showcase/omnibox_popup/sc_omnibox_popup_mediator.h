// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_MEDIATOR_H_
#define IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_MEDIATOR_H_

#include <UIKit/UIKit.h>

@protocol AutocompleteResultConsumer;

// Mediator for the omnibox popup entry in Showcase.
@interface SCOmniboxPopupMediator : NSObject

- (instancetype)initWithConsumer:(id<AutocompleteResultConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Tell the consumer to update its matches with those provided by this provider.
- (void)updateMatches;

@end

#endif  // IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_MEDIATOR_H_
