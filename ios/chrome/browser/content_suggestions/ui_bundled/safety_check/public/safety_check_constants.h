// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_PUBLIC_SAFETY_CHECK_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_PUBLIC_SAFETY_CHECK_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace base {
class TimeDelta;
}  // namespace base

namespace safety_check {

// Accessibility IDs used for various UI items.
extern NSString* const kAllSafeItemID;
extern NSString* const kRunningItemID;
extern NSString* const kUpdateChromeItemID;
extern NSString* const kPasswordItemID;
extern NSString* const kSafeBrowsingItemID;
extern NSString* const kDefaultItemID;
extern NSString* const kSafetyCheckViewID;

// Duration between each autorun of the Safety Check in the Magic Stack.
extern const base::TimeDelta kTimeDelayForSafetyCheckAutorun;

}  // namespace safety_check

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_PUBLIC_SAFETY_CHECK_CONSTANTS_H_
