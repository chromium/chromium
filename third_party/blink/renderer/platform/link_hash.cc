/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/link_hash.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/url_util.h"

namespace blink {

static bool ResolveRelative(const KURL& base,
                            const String& relative,
                            url::RawCanonOutput<2048>* buffer) {
  // We use these low-level GURL functions to avoid converting back and forth
  // from UTF-8 unnecessarily.
  url::Parsed parsed;
  StringUTF8Adaptor base_utf8(base.GetString());
  if (relative.Is8Bit()) {
    StringUTF8Adaptor relative_utf8(relative);
    return url::ResolveRelative(base_utf8.data(), base_utf8.size(),
                                base.GetParsed(), relative_utf8.data(),
                                relative_utf8.size(), nullptr, buffer, &parsed);
  }
  return url::ResolveRelative(base_utf8.data(), base_utf8.size(),
                              base.GetParsed(), relative.Characters16(),
                              relative.length(), nullptr, buffer, &parsed);
}

LinkHash VisitedLinkHash(const KURL& base, const AtomicString& relative) {
  if (relative.IsNull())
    return 0;
  url::RawCanonOutput<2048> buffer;
  if (!ResolveRelative(base, relative.GetString(), &buffer))
    return 0;
  return Platform::Current()->VisitedLinkHash(buffer.data(), buffer.length());
}

}  // namespace blink
