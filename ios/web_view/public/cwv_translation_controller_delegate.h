// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_DELEGATE_H
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_DELEGATE_H

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVTranslationController;
@class CWVTranslationLanguage;

// Updates delegate on translation progress.
@protocol CWVTranslationControllerDelegate<NSObject>

@optional
// Called if the current page is not automatically translated, but may need
// translation according to its language and the user's locale.
// |pageLanguage| is given as the detected language of the page and
// |userLanguage| is given as the best guess for the preferred native language.
- (void)translationController:(CWVTranslationController*)controller
    canOfferTranslationFromLanguage:(CWVTranslationLanguage*)pageLanguage
                         toLanguage:(CWVTranslationLanguage*)userLanguage;

// Called when a translation has started. |userInitiated| is YES if this
// translation was started explicitly by user action, and NO if it was started
// automatically because of a translation policy with CWVTranslationPolicyAuto.
// If translation was started by a call to CWVTranslationController's
// |translatePageFromLanguage:toLanguage:userInitiated:| method, |userInitiated|
// will be the same as the value passed to that method.
- (void)translationController:(CWVTranslationController*)controller
    didStartTranslationFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                         toLanguage:(CWVTranslationLanguage*)targetLanguage
                      userInitiated:(BOOL)userInitiated;

// Called when translation finishes. |error| will be nonnull if it failed.
- (void)translationController:(CWVTranslationController*)controller
    didFinishTranslationFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                          toLanguage:(CWVTranslationLanguage*)targetLanguage
                               error:(nullable NSError*)error;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_DELEGATE_H
