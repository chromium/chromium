// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_

// Class that stores contents of prefs sent by the source of the
// InfobarTranslateModalConsumer.
@interface FakeInfobarTranslateModalConsumer
    : NSObject <InfobarTranslateModalConsumer>
// The source language from which to translate.
@property(nonatomic, copy) NSString* sourceLanguage;
// The target language to which to translate.
@property(nonatomic, copy) NSString* targetLanguage;
// Whether the source language is auto unknown.
@property(nonatomic, assign) BOOL sourceLanguageIsUnknown;

// YES if the pref is set to enable the Translate button.
@property(nonatomic, assign) BOOL enableTranslateActionButton;
// YES if the pref is set to display the "Show Original" Button.
@property(nonatomic, assign) BOOL displayShowOriginalButton;
// YES if the pref is set to show the "Never Translate language" button.
@property(nonatomic, assign) BOOL shouldDisplayNeverTranslateLanguageButton;
// YES if the pref is set to show the "Never Translate Site" button.
@property(nonatomic, assign) BOOL shouldDisplayNeverTranslateSiteButton;

// YES if the pref is set to configure the Translate button to trigger
// -translateWithNewLanguages.
@property(nonatomic, assign) BOOL updateLanguageBeforeTranslate;

// YES if the pref is set to always translate for the source language.
@property(nonatomic, assign) BOOL shouldAlwaysTranslate;
// NO if the current pref is set to never translate the source language.
@property(nonatomic, assign) BOOL isTranslatableLanguage;
// YES if the pref is set to never translate the current site.
@property(nonatomic, assign) BOOL isSiteOnNeverPromptList;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_
