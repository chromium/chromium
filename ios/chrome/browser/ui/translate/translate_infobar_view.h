// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_VIEW_H_

#import <UIKit/UIKit.h>

#include <vector>

// States in which the infobar can be.
typedef NS_ENUM(NSInteger, TranslateInfobarViewState) {
  TranslateInfobarViewStateBeforeTranslate,
  TranslateInfobarViewStateTranslating,
  TranslateInfobarViewStateAfterTranslate,
};

// Height of the infobar.
extern const CGFloat kInfobarHeight;

// The a11y identifier for the translate infobar view.
extern NSString* const kTranslateInfobarViewId;

@protocol TranslateInfobarViewDelegate;

// An infobar for translating the page. Starting from the leading edge, it
// features an icon followed by the source and the target languages. Toggling
// between the languages results in the page to be translated or the translation
// to be reverted. At the trailing edge, the infobar features an options button
// that opens a popup menu that allows changing translate preferences followed
// by a dismiss button to close the infobar.
@interface TranslateInfobarView : UIView

// Source language name.
@property(nonatomic, copy) NSString* sourceLanguage;

// Target language name.
@property(nonatomic, copy) NSString* targetLanguage;

// Infobar's state.
@property(nonatomic) TranslateInfobarViewState state;

// Delegate object that gets notified if user taps the source language tab, the
// target language tab, the options button, or the dismiss button.
@property(nonatomic, weak) id<TranslateInfobarViewDelegate> delegate;

// Updates the infobar UI when a popup menu is displayed or dismissed.
- (void)updateUIForPopUpMenuDisplayed:(BOOL)displayed;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_VIEW_H_
