// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_FAKE_AT_MEMORY_FILL_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_FAKE_AT_MEMORY_FILL_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"

// Fake implementation of `AtMemoryFillCommands` for testing.
@interface FakeAtMemoryFillHandler : NSObject <AtMemoryFillCommands>

// Whether `fillWithSuggestion:` was called.
@property(nonatomic, assign) BOOL fillWithSuggestionCalled;

// Whether `fillWithContent:` was called.
@property(nonatomic, assign) BOOL fillWithContentCalled;

// The last content passed to `fillWithContent:`.
@property(nonatomic, copy) NSString* filledContent;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_FAKE_AT_MEMORY_FILL_HANDLER_H_
