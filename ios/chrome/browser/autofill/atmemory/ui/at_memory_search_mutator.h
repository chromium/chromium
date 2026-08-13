// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator for the AtMemorySearchViewController to update the
// AtMemorySearchMediator.
@protocol AtMemorySearchMutator <NSObject>

// Called when the user initiates a search with the given `query`.
- (void)startSearchWithQuery:(NSString*)query;

// Called when the user acknowledges the informational notice.
- (void)acknowledgePrivacyNotice;

// Called when the user clicks the Settings link in the notice.
- (void)didTapSettingsLink;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_MUTATOR_H_
