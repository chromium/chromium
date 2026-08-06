// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_SEARCH_RESULT_COMMANDS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_SEARCH_RESULT_COMMANDS_H_

namespace autofill {
struct MemorySearchResult;
}

// Commands handler for AtMemory search result actions.
@protocol AtMemorySearchResultCommands <NSObject>

// Shows the AtMemory granular fill UI for the given search result.
- (void)showAtMemoryGranularFillWithResult:
    (const autofill::MemorySearchResult&)result;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_SEARCH_RESULT_COMMANDS_H_
