// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/url_conversion.h"

#include <string_view>

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"

namespace blink {

GURL WebStringToGURL(const WebString& web_string) {
  if (web_string.IsEmpty())
    return GURL();

  String str = web_string;
  if (str.Is8Bit()) {
    // Ensure the (possibly Latin-1) 8-bit string is UTF-8 for GURL.
    StringUTF8Adaptor utf8(str);
    return GURL(utf8.AsStringView());
  }

  // GURL can consume UTF-16 directly.
  return GURL(std::u16string_view(str.Characters16(), str.length()));
}

}  // namespace blink
