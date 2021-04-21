// Copyright 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WIN_WEB_FONT_RENDERING_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WIN_WEB_FONT_RENDERING_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkFontMgr;
class SkTypeface;

namespace blink {

class WebFontRendering {
 public:
  BLINK_EXPORT static void SetSkiaFontManager(sk_sp<SkFontMgr>);
  BLINK_EXPORT static void AddSideloadedFontForTesting(sk_sp<SkTypeface>);
  BLINK_EXPORT static void SetMenuFontMetrics(const WebString& family_name,
                                              int32_t font_height);
  BLINK_EXPORT static void SetSmallCaptionFontMetrics(
      const WebString& family_name,
      int32_t font_height);
  BLINK_EXPORT static void SetStatusFontMetrics(const WebString& family_name,
                                                int32_t font_height);
  BLINK_EXPORT static void SetAntialiasedTextEnabled(bool);
  BLINK_EXPORT static void SetLCDTextEnabled(bool);
  BLINK_EXPORT static void SetUseSkiaFontFallback(bool);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WIN_WEB_FONT_RENDERING_H_
