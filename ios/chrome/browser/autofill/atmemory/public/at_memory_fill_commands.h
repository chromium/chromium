// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_FILL_COMMANDS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_FILL_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct Suggestion;
}

// Commands handler for AtMemory fill actions.
@protocol AtMemoryFillCommands <NSObject>

// Fills the active form field with the given `content`.
- (void)fillWithContent:(NSString*)content;

// Fills the active form field with the given `suggestion`.
- (void)fillWithSuggestion:(const autofill::Suggestion&)suggestion;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_FILL_COMMANDS_H_
