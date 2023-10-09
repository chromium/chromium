// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_MATCH_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_MATCH_H_

#import <Foundation/Foundation.h>

// Object describing the locale codes that match a single supported locale.
@interface SpeechInputLocaleMatch : NSObject

// Designated initializer.
- (instancetype)initWithMatchedLocale:(NSString*)matchedLocale
                      matchingLocales:(NSArray<NSString*>*)matchingLocales
                    matchingLanguages:(NSArray<NSString*>*)matchingLanguages
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The locale code that should be used as the default code for the locale codes
// found in `matchingLocales`.
@property(nonatomic, readonly) NSString* matchedLocale;

// The locale codes that should be matched with `matchedLocale`.
@property(nonatomic, readonly) NSArray<NSString*>* matchingLocales;

// The languages that use `matchedLocale` as a default.
@property(nonatomic, readonly) NSArray<NSString*>* matchingLanguages;

@end

// Loads matching locales for unsuppoered regional variants from
// SpeechInputLocalesMatches.plist.
NSArray<SpeechInputLocaleMatch*>* LoadSpeechInputLocaleMatches();

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_MATCH_H_
