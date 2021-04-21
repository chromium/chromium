/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_HTTP_LOAD_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_HTTP_LOAD_INFO_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

namespace blink {

class WebString;
struct ResourceLoadInfo;

class WebHTTPLoadInfo {
 public:
  WebHTTPLoadInfo() { Initialize(); }
  ~WebHTTPLoadInfo() { Reset(); }
  WebHTTPLoadInfo(const WebHTTPLoadInfo& r) { Assign(r); }
  WebHTTPLoadInfo& operator=(const WebHTTPLoadInfo& r) {
    Assign(r);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Initialize();
  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebHTTPLoadInfo& r);

  BLINK_PLATFORM_EXPORT int HttpStatusCode() const;
  BLINK_PLATFORM_EXPORT void SetHTTPStatusCode(int);

  BLINK_PLATFORM_EXPORT WebString HttpStatusText() const;
  BLINK_PLATFORM_EXPORT void SetHTTPStatusText(const WebString&);

  BLINK_PLATFORM_EXPORT void AddRequestHeader(const WebString& name,
                                              const WebString& value);
  BLINK_PLATFORM_EXPORT void AddResponseHeader(const WebString& name,
                                               const WebString& value);

  BLINK_PLATFORM_EXPORT WebString RequestHeadersText() const;
  BLINK_PLATFORM_EXPORT void SetRequestHeadersText(const WebString&);

  BLINK_PLATFORM_EXPORT WebString ResponseHeadersText() const;
  BLINK_PLATFORM_EXPORT void SetResponseHeadersText(const WebString&);

  BLINK_PLATFORM_EXPORT WebString NpnNegotiatedProtocol() const;
  BLINK_PLATFORM_EXPORT void SetNPNNegotiatedProtocol(const WebString&);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebHTTPLoadInfo(scoped_refptr<ResourceLoadInfo>);
  BLINK_PLATFORM_EXPORT operator scoped_refptr<ResourceLoadInfo>() const;
#endif

 private:
  WebPrivatePtr<ResourceLoadInfo> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_HTTP_LOAD_INFO_H_
