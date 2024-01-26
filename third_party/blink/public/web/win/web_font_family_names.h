// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WIN_WEB_FONT_FAMILY_NAMES_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WIN_WEB_FONT_FAMILY_NAMES_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// Used to record the font family names needed to render a frame.
struct BLINK_EXPORT WebFontFamilyNames {
  WebVector<WebString> font_names;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WIN_WEB_FONT_FAMILY_NAMES_H_
