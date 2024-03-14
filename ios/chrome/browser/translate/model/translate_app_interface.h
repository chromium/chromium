// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@interface TranslateAppInterface : NSObject

// Sets up the app for testing. `translateScriptServer` is the URL
// for the server that can serve up translate scripts to app.
+ (void)setUpWithScriptServer:(NSString*)translateScriptServerURL;

// Tears down the testing set up for the app.
+ (void)tearDown;

// Registers an observer of the IOSLanguageDetectionTabHelper to capture
// the details of webpage language detection. The captured details can be
// used for test verification.
+ (void)setUpLanguageDetectionTabHelperObserver;

// Deallocates the observer for IOSLanguageDetectionTabHelper.
+ (void)tearDownLanguageDetectionTabHelperObserver;

// Resets the language detection state kept in the observer of
// IOSLanguageDetectionTabHelper.
+ (void)resetLanguageDetectionTabHelperObserver;

// Returns whether a language information was detected on the webpage.
+ (BOOL)isLanguageDetected;

// Returns the language code of the webpage indicated in the Content-Language
// HTTP header.
+ (NSString*)contentLanguage;

// Returns the language code indicated in the language attribute of the HTML
// element.
+ (NSString*)htmlRootLanguage;

// Returns the language code for the language determined from the webpage.
+ (NSString*)adoptedLanguage;

/// Whether user has set a preference to translate from `source` language to
// `target` language.
+ (BOOL)shouldAutoTranslateFromLanguage:(NSString*)source
                             toLanguage:(NSString*)target;

// Whether user has set a preference to block the translation of `language`.
+ (BOOL)isBlockedLanguage:(NSString*)language;

// Whether user has set a preference to translate any pages on `hostName`.
+ (BOOL)isBlockedSite:(NSString*)hostName;

// The following are Translate Infobar UI constants. Test client needs to know
// to verify that Translate Infobar is behaving correctly.
+ (int)infobarAutoAlwaysThreshold;
+ (int)infobarAutoNeverThreshold;
+ (int)infobarMaximumNumberOfAutoAlways;
+ (int)infobarMaximumNumberOfAutoNever;

@end

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_APP_INTERFACE_H_
