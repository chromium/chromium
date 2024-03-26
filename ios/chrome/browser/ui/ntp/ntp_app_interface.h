// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NTP_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NTP_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

enum class ContentSuggestionsModuleType;

// A container for Home test cases to log relevant state changes or actions.
@interface NTPAppInterface : NSObject

+ (void)recordModuleFreshnessSignalForType:
    (ContentSuggestionsModuleType)module_type;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NTP_APP_INTERFACE_H_
