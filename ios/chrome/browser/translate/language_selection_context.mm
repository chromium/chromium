// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/language_selection_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LanguageSelectionContext ()
// Redeclare public properties readwrite.
@property(nonatomic, readwrite)
    const translate::TranslateInfoBarDelegate* languageData;
@property(nonatomic, readwrite) size_t initialLanguageIndex;
@property(nonatomic, readwrite) size_t unavailableLanguageIndex;
@end

@implementation LanguageSelectionContext

@synthesize languageData = _languageData;
@synthesize initialLanguageIndex = _initialLanguageIndex;
@synthesize unavailableLanguageIndex = _unavailableLanguageIndex;

+ (instancetype)contextWithLanguageData:
                    (translate::TranslateInfoBarDelegate*)languageData
                           initialIndex:(size_t)initialLanguageIndex
                       unavailableIndex:(size_t)unavailableLanguageIndex {
  LanguageSelectionContext* context = [[LanguageSelectionContext alloc] init];
  context.languageData = languageData;
  context.initialLanguageIndex = initialLanguageIndex;
  context.unavailableLanguageIndex = unavailableLanguageIndex;
  return context;
}

@end
