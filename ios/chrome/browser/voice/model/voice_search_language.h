// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_LANGUAGE_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_LANGUAGE_H_

#import <Foundation/Foundation.h>

// VoiceSearchLanguage stores data about a single supported voice search
// language.
@interface VoiceSearchLanguage : NSObject

// Creates a VoiceSearchLanguageObject.  `localizationPreferences` can be nil.
- (instancetype)initWithIdentifier:(NSString*)identifier
                       displayName:(NSString*)displayName
            localizationPreference:(NSString*)localizationPreference
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The BCP 47 identifier for the language, e.g. "en-us" or "yue-hant-hk".
@property(nonatomic, readonly, copy) NSString* identifier;

// The display name for the language.
@property(nonatomic, readonly, copy) NSString* displayName;

// A localization identifier for use with +[NSBundle
// preferredLocalizationsFromArray:forPreferences:].  In general this is the
// same as `identifier`, but in some cases e.g.  Chinese and Cantonese it
// differs to facilitate the NSBundle method.
@property(nonatomic, readonly, copy) NSString* localizationPreference;

@end

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_LANGUAGE_H_
