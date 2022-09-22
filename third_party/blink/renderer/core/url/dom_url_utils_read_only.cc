/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/url/dom_url_utils_read_only.h"

#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

String DOMURLUtilsReadOnly::href() {
  const KURL& kurl = Url();
  if (kurl.IsNull())
    return Input();
  return kurl.GetString();
}

String DOMURLUtilsReadOnly::origin(const KURL& kurl) {
  if (kurl.IsNull())
    return "";
  return SecurityOrigin::Create(kurl)->ToString();
}

String DOMURLUtilsReadOnly::host(const KURL& kurl) {
  if (kurl.HostEnd() == kurl.PathStart())
    return kurl.Host();
  if (IsDefaultPortForProtocol(kurl.Port(), kurl.Protocol()))
    return kurl.Host();
  return kurl.Host() + ":" + String::Number(kurl.Port());
}

String DOMURLUtilsReadOnly::port(const KURL& kurl) {
  if (kurl.HasPort())
    return String::Number(kurl.Port());

  return g_empty_string;
}

String DOMURLUtilsReadOnly::search(const KURL& kurl) {
  String query = kurl.Query();
  return query.IsEmpty() ? g_empty_string : "?" + query;
}

String DOMURLUtilsReadOnly::hash(const KURL& kurl) {
  String fragment_identifier = kurl.FragmentIdentifier();
  if (fragment_identifier.IsEmpty())
    return g_empty_string;
  return AtomicString(String("#" + fragment_identifier));
}

}  // namespace blink
