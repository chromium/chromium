// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_LANGUAGE_DETECTION_DETAILS_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_LANGUAGE_DETECTION_DETAILS_INTERNAL_H_

#import "ios/web_view/public/cwv_translation_language_detection_details.h"

NS_ASSUME_NONNULL_BEGIN

@interface CWVTranslationLanguageDetectionDetails ()

+ (instancetype)languageDetectionDetailsFrom:
    (const translate::LanguageDetectionDetails&)details;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_LANGUAGE_DETECTION_DETAILS_INTERNAL_H_
