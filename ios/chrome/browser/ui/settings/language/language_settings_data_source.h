// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_DATA_SOURCE_H_

#include <Foundation/Foundation.h>


@class LanguageItem;
@protocol LanguageSettingsConsumer;

// The data source protocol for the Language Settings page.
@protocol LanguageSettingsDataSource

// Returns the accept languages list ordered according to the user prefs.
- (NSArray<LanguageItem*>*)acceptLanguagesItems;

// Returns the supported languages list excluding the accept languages. This
// list is sorted alphabetically based on display names in the current locale.
- (NSArray<LanguageItem*>*)supportedLanguagesItems;

// Returns whether or not Translate is enabled.
- (BOOL)translateEnabled;

// Returns whether or not Translate is managed by enterprise policy.
- (BOOL)translateManaged;

// Stops observing the model. This is required during the shutdown.
- (void)stopObservingModel;

// The consumer for this protocol.
@property(nonatomic, weak) id<LanguageSettingsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_DATA_SOURCE_H_
