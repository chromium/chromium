// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LANGUAGE_DETECTION_DETAILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LANGUAGE_DETECTION_DETAILS_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

class WebDocument;

struct WebLanguageDetectionDetails {
  WebURL url;
  WebString content_language;
  WebString html_language;
  bool has_no_translate_meta = false;

  BLINK_EXPORT static WebLanguageDetectionDetails
  CollectLanguageDetectionDetails(const WebDocument&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LANGUAGE_DETECTION_DETAILS_H_
