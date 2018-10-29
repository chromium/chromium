/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ICON_URL_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ICON_URL_H_

#if INSIDE_BLINK
#include "third_party/blink/renderer/core/dom/icon_url.h"  // nogncheck
#endif
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

class WebIconURL {
 public:
  enum Type {
    kTypeInvalid = 0,
    kTypeFavicon = 1 << 0,
    kTypeTouch = 1 << 1,
    kTypeTouchPrecomposed = 1 << 2
  };

  WebIconURL() : icon_type_(kTypeInvalid) {}

  WebIconURL(const WebURL& url, Type type) : icon_type_(type), icon_url_(url) {}

  Type IconType() const { return icon_type_; }

  const WebURL& GetIconURL() const { return icon_url_; }

  const WebVector<WebSize>& Sizes() const { return sizes_; }

#if INSIDE_BLINK
  WebIconURL(const IconURL& icon_url)
      : icon_type_(static_cast<Type>(icon_url.icon_type_)),
        icon_url_(icon_url.icon_url_),
        sizes_(icon_url.sizes_) {}
#endif

 private:
  Type icon_type_;
  WebURL icon_url_;
  WebVector<WebSize> sizes_;
};
}

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ICON_URL_H_
