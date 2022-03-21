// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_ACCEPT_LANGUAGE_AND_CONTENT_LANGUAGE_USAGE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_ACCEPT_LANGUAGE_AND_CONTENT_LANGUAGE_USAGE_H_

namespace blink {

// Used to record the matches between the Content-Language response header value
// and the Accept-Language request header values.
//
// Corresponds to the "ContentLanguageCase" histogram enumeration type in
// tools/metrics/histograms/enums.xml.
//
// PLEASE DO NOT REORDER, REMOVE, OR CHANGE THE MEANING OF THESE VALUES.
enum class AcceptLanguageAndContentLanguageUsage {
  kContentLanguageEmpty,
  kContentLanguageWildcard,
  kContentLanguageMatchesAnyAcceptLanguage,
  kContentLanguageMatchesPrimaryAcceptLanguage,
  kContentLanguageSubframeDiffers,
  kMaxValue = kContentLanguageSubframeDiffers
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_ACCEPT_LANGUAGE_AND_CONTENT_LANGUAGE_USAGE_H_"
