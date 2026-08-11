// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct MemorySearchResult;
}

// Returns the granular fill title for `result`.
NSString* GetAtMemoryGranularFillTitle(
    const autofill::MemorySearchResult& result);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
