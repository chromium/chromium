// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_PREFERENCES_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_PREFERENCES_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Preferences for user settings. The preferences are stored on the local
// storage.
CWV_EXPORT
@interface CWVPreferences : NSObject

// Whether or not translation as a feature is turned on. Defaults to |YES|.
// Because translate settings are shared from incognito to non-incognito, this
// has no effect if this instance is from an incognito CWVWebViewConfiguration.
@property(nonatomic, assign, getter=isTranslationEnabled)
    BOOL translationEnabled;

// Whether or not profile autofill is turned on. Defaults to |YES|.
// If enabled, contents of submitted profiles may be saved and offered as a
// suggestion in either the same or similar forms.
@property(nonatomic, assign, getter=isProfileAutofillEnabled)
    BOOL profileAutofillEnabled;

// Whether or not credit card autofill is turned on. Defaults to |YES|.
// If enabled, contents of submitted credit cards may be saved and offered as a
// suggestion in either the same or similar forms.
@property(nonatomic, assign, getter=isCreditCardAutofillEnabled)
    BOOL creditCardAutofillEnabled;

// Whether or not CWVWebView allows saving passwords for autofill. Defaults to
// |YES|. When it is NO, it doesn't ask if you want to save passwords but will
// continue to fill passwords.
//
// TODO(crbug.com/40602365): Preference should also control autofill behavior
// for the passwords.
@property(nonatomic, assign, getter=isPasswordAutofillEnabled)
    BOOL passwordAutofillEnabled;

// Whether or not password leak checks will be performed after successful form
// submission. Defaults to |YES|.
@property(nonatomic, assign, getter=isPasswordLeakCheckEnabled)
    BOOL passwordLeakCheckEnabled;

// Whether or not safe browsing is enabled.
// Specifically this controls whether or not
// -[CWVNavigationDelegate handleUnsafeURLWithHandler:] is called.
// Defaults to |YES|.
@property(nonatomic, assign, getter=isSafeBrowsingEnabled)
    BOOL safeBrowsingEnabled;

- (instancetype)init NS_UNAVAILABLE;

// Resets all translation settings back to default. In particular, this will
// change all translation policies back to CWVTranslationPolicyAsk, and set
// |translationEnabled| to YES. Because translate settings are shared from
// incognito to non-incognito, this has no effect if this instance is from an
// incognito CWVWebViewConfiguration.
- (void)resetTranslationSettings;

// Immediately writes any changes in memory to disk.
// `completionHandler` callback when writes are committed.
- (void)commitPendingWrite:(void (^)(void))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_PREFERENCES_H_
