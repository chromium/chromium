// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_COMMANDS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_COMMANDS_H_

#import "components/autofill/core/common/unique_ids.h"

@protocol AtMemoryCommands <NSObject>

// Commands the parent coordinator to show the AtMemory UI targeted at the
// specified field.
- (void)showAtMemoryForField:(autofill::FieldGlobalId)fieldId;

// Commands the parent coordinator to dismiss the AtMemory UI.
- (void)dismissAtMemory;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_COMMANDS_H_
