// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UTILS_AUTOFILL_AI_DATE_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UTILS_AUTOFILL_AI_DATE_UTIL_H_

#import <Foundation/Foundation.h>

#import <string>

namespace autofill {
class AttributeInstance;
}

// Returns a NSDate representing the value of the date attribute `attribute`.
// Returns nil if `attribute` is not a date type or if the date is invalid.
NSDate* NSDateFromAttributeInstance(
    const autofill::AttributeInstance& attribute);

// Returns a string representation of `date` in the format expected by
// `autofill::AttributeInstance::SetInfo()`.
std::u16string AttributeValueFromNSDate(NSDate* date);

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UTILS_AUTOFILL_AI_DATE_UTIL_H_
