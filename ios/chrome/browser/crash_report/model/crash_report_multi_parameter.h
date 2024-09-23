// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_MULTI_PARAMETER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_MULTI_PARAMETER_H_

#import <Foundation/Foundation.h>

#include <string_view>

#include "components/crash/core/common/crash_key.h"

// CrashReportMultiParameter keeps state of multiple report values that will be
// grouped in a single crash key element to save limited number of crash key
// values.
// TOOD(crbug.com/1412774): This class dates back to Breakpad and is no longer
// needed with Crashpad. Migrate this to individual crash keys instead.
@interface CrashReportMultiParameter : NSObject
// Init with the crash key parameter.
- (instancetype)initWithKey:(crash_reporter::CrashKeyString<256>&)key
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds or updates an element in the dictionary of the crash report. Value
// are stored in the instance so every time you call this function, the whole
// JSON dictionary is regenerated. The total size of the serialized dictionary
// must be under 512 bytes. Setting a value to 0 will not remove the key.
// Use [removeValue:key] instead.
- (void)setValue:(std::string_view)key withValue:(int)value;

// Removes the key element from the dictionary. Note that this is different from
// setting the parameter to 0 or false.
- (void)removeValue:(std::string_view)key;

// Increases the key element by one.
- (void)incrementValue:(std::string_view)key;

// Decreases the key element by one. If the element is 0, the element is removed
// from the dictionary.
- (void)decrementValue:(std::string_view)key;
@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_MULTI_PARAMETER_H_
