// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_COORDINATOR_TAB_RESUMPTION_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_COORDINATOR_TAB_RESUMPTION_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

enum class ContentSuggestionsModuleType;

// Delegate handling events from the TabResumptionMediator.
@protocol TabResumptionMediatorDelegate

// Signals that the TabResumptionMediator received a new item configuration.
- (void)tabResumptionMediatorDidReceiveItem;

// Signals that the TabResumptionMediator did reconfignure the existing item.
- (void)tabResumptionMediatorDidReconfigureItem;

// Signals that the Tab Resumption module should be removed.
- (void)removeTabResumptionModule;

// Logs a user Magic Stack engagement for module `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

// Returns the index rank of `moduleType` or NSNotFound if not found.
- (NSUInteger)indexForMagicStackModule:(ContentSuggestionsModuleType)moduleType;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_COORDINATOR_TAB_RESUMPTION_MEDIATOR_DELEGATE_H_
