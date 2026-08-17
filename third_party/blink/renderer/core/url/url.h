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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_URL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_URL_H_

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/url/dom_origin_utils.h"
#include "third_party/blink/renderer/core/url/url_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Blob;
class ExceptionState;
class ExecutionContext;
class MediaSourceAttachment;
class URLSearchParams;

class CORE_EXPORT URL final : public ScriptWrappable,
                              public UrlUtils,
                              public DOMOriginUtils {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using PassKey = base::PassKey<URL>;

  static URL* Create(const String& url, ExceptionState& exception_state);
  static URL* Create(const String& url,
                     const String& base,
                     ExceptionState& exception_state);

  URL(PassKey, const String& url, const KURL& base, ExceptionState&);
  URL(PassKey, const KURL& url);
  ~URL() override;

  // DOMOriginUtils overrides:
  DOMOrigin* GetDOMOrigin(LocalDOMWindow*) const override;

  static URL* parse(const String& url);
  static URL* parse(const String& url, const String& base);

  static bool canParse(const String& url);
  static bool canParse(const String& url, const String& base);

  static String CreatePublicURL(ExecutionContext*, Blob*);
  static String CreatePublicURL(ExecutionContext*,
                                scoped_refptr<MediaSourceAttachment>);

  KURL Url() const override { return url_; }
  void SetUrl(const KURL& url) override { url_ = url; }

  String Input() const override {
    // Url() can never be null, so Input() is never called.
    NOTREACHED();
  }

  void setHref(const String&, ExceptionState& exception_state);
  void setSearch(const String&) override;

  URLSearchParams* searchParams();

  String toJSON() { return href(); }

  void Trace(Visitor*) const override;

 private:
  friend class URLSearchParams;

  void Update();
  void UpdateSearchParams(const String&);

  KURL url_;
  WeakMember<URLSearchParams> search_params_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_URL_H_
