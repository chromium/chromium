// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_MEDIATOR_H_

#include <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/language/language_settings_commands.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_data_source.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@interface LanguageSettingsMediator
    : NSObject <LanguageSettingsDataSource, LanguageSettingsCommands>

// The designated initializer. |browserState| must not be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_MEDIATOR_H_
