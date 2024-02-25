// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_LANGUAGE_SELECTION_CONTEXT_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_LANGUAGE_SELECTION_CONTEXT_H_

#import <Foundation/Foundation.h>

namespace translate {
class TranslateInfoBarDelegate;
}

// Context information for a language selection event.
@interface LanguageSelectionContext : NSObject

// Convenience initializer that populates all of the properties of the returned
// object with the passed parameters.
+ (instancetype)contextWithLanguageData:
                    (translate::TranslateInfoBarDelegate*)languageData
                           initialIndex:(size_t)initialLanguageIndex
                       unavailableIndex:(size_t)unavailableLanguageIndex;

// The object that provides language data for the selection.
@property(nonatomic, readonly)
    const translate::TranslateInfoBarDelegate* languageData;
// The index of the language that's initially selected.
@property(nonatomic, readonly) size_t initialLanguageIndex;
// The index of the language that can't be selected (because this context is
// for source selection and this is the index of the target language, or
// vice-versa).
@property(nonatomic, readonly) size_t unavailableLanguageIndex;

@end

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_LANGUAGE_SELECTION_CONTEXT_H_
