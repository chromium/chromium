// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVTranslationLanguage;
@class CWVTranslationPolicy;
@protocol CWVTranslationControllerDelegate;

// The error domain for translation errors.
FOUNDATION_EXPORT CWV_EXPORT NSErrorDomain const CWVTranslationErrorDomain;

// Possible error codes during translation.
typedef NS_ENUM(NSInteger, CWVTranslationError) {
  // No error.
  CWVTranslationErrorNone = 0,
  // No connectivity.
  CWVTranslationErrorNetwork,
  // The translation script failed to initialize.
  CWVTranslationErrorInitializationError,
  // The page's language could not be detected.
  CWVTranslationErrorUnknownLanguage,
  // The server detected a language that the browser does not know.
  CWVTranslationErrorUnsupportedLanguage,
  // The original and target languages are the same.
  CWVTranslationErrorIdenticalLanguages,
  // An error was reported by the translation script during translation.
  CWVTranslationErrorTranslationError,
  // The library doesn't finish the translation.
  CWVTranslationErrorTranslationTimeout,
  // The library raises an unexpected exception.
  CWVTranslationErrorUnexpectedScriptError,
  // The library is blocked because of bad origin.
  CWVTranslationErrorBadOrigin,
  // Loader fails to load a dependent JavaScript.
  CWVTranslationErrorScriptLoadError,
};

// Allows page translation from one language to another.
CWV_EXPORT
@interface CWVTranslationController : NSObject

// Delegate to receive translation callbacks.
@property(nullable, nonatomic, weak) id<CWVTranslationControllerDelegate>
    delegate;

// The set of supported languages for translation.
@property(nonatomic, readonly)
    NSSet<CWVTranslationLanguage*>* supportedLanguages;

- (instancetype)init NS_UNAVAILABLE;

// Begins translation on the current page from |sourceLanguage| to
// |targetLanguage|. These language parameters must be chosen from
// |supportedLanguages|. Set |userInitiated| to YES if translation
// is a result of explicit user action. |userInitiated| will be
// passed along to the CWVTranslationControllerDelegate methods.
// Results in a No-op if there is no current page.
- (void)translatePageFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                       toLanguage:(CWVTranslationLanguage*)targetLanguage
                    userInitiated:(BOOL)userInitiated;

// Reverts any translations done back to the original page language.
// Note that the original page language may be different from |sourceLanguage|
// passed to |translatePageFromLanguage:toLanguage:userInitiated:| above.
// Results in No-op if the page was never translated.
- (void)revertTranslation;

// If the |delegate| was not offered to translate the page via the method
// |translationController:canOfferTranslationFromLanguage:toLanguage:|, this
// method may be called to manually trigger it.
// Returns boolean indicating if a translation can be offered.
- (BOOL)requestTranslationOffer;

// Sets or retrieves translation policies associated with a specified language.
// |pageLanguage| should be the language code of the language.
- (void)setTranslationPolicy:(CWVTranslationPolicy*)policy
             forPageLanguage:(CWVTranslationLanguage*)pageLanguage;
- (CWVTranslationPolicy*)translationPolicyForPageLanguage:
    (CWVTranslationLanguage*)pageLanguage;

// Sets or retrieves translation policies associated with a specified page.
// |pageHost| should be the hostname of the website. Must not be empty.
- (void)setTranslationPolicy:(CWVTranslationPolicy*)policy
                 forPageHost:(NSString*)pageHost;
- (CWVTranslationPolicy*)translationPolicyForPageHost:(NSString*)pageHost;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_H_
