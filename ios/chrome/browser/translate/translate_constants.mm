// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kLanguageHistogramTranslate[] =
    "Translate.CompactInfobar.Language.Translate";
const char kLanguageHistogramMoreLanguages[] =
    "Translate.CompactInfobar.Language.MoreLanguages";
const char kLanguageHistogramPageNotInLanguage[] =
    "Translate.CompactInfobar.Language.PageNotIn";
const char kLanguageHistogramAlwaysTranslate[] =
    "Translate.CompactInfobar.Language.AlwaysTranslate";
const char kLanguageHistogramNeverTranslate[] =
    "Translate.CompactInfobar.Language.NeverTranslate";
const char kEventHistogram[] = "Translate.CompactInfobar.Event";
const char kTranslationCountHistogram[] =
    "Translate.CompactInfobar.TranslationsPerPage";
