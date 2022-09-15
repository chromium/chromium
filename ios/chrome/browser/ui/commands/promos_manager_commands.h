// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_PROMOS_MANAGER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_PROMOS_MANAGER_COMMANDS_H_

#import "ios/chrome/browser/promos_manager/constants.h"

// Commands to show app-wide promos.
@protocol PromosManagerCommands <NSObject>

// Displays an eligible promo (selected by the Promos Manager) if one exists.
- (void)maybeDisplayPromo;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_PROMOS_MANAGER_COMMANDS_H_
