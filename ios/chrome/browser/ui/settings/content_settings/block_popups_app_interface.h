// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_BLOCK_POPUPS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_BLOCK_POPUPS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#include "components/content_settings/core/common/content_settings.h"

// BlockPopupsAppInterface provides app-side helpers for BlockPopupsTest.
@interface BlockPopupsAppInterface : NSObject

// Sets the popup content setting policy for the given `pattern`.
+ (void)setPopupPolicy:(ContentSetting)policy forPattern:(NSString*)pattern;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_BLOCK_POPUPS_APP_INTERFACE_H_
