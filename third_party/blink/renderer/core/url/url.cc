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

#include "third_party/blink/renderer/core/url/url.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/url/dom_origin.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
URL* URL::Create(const String& url, ExceptionState& exception_state) {
  return MakeGarbageCollected<URL>(PassKey(), url, NullUrl(), exception_state);
}

// static
URL* URL::Create(const String& url,
                 const String& base,
                 ExceptionState& exception_state) {
  KURL base_url(base);
  if (!base_url.IsValid()) {
    exception_state.ThrowTypeError("Invalid base URL");
    return nullptr;
  }
  return MakeGarbageCollected<URL>(PassKey(), url, base_url, exception_state);
}

URL::URL(PassKey,
         const String& url,
         const KURL& base,
         ExceptionState& exception_state)
    : url_(base, url) {
  if (!url_.IsValid()) {
    exception_state.ThrowTypeError("Invalid URL");
  }
}

URL::URL(PassKey, const KURL& url) : url_(url) {}

URL::~URL() = default;

DOMOrigin* URL::GetDOMOrigin(LocalDOMWindow*) const {
  // No access check is required, as URLs are not accessible cross-origin.
  return DOMOrigin::Create(SecurityOrigin::Create(Url()));
}

void URL::Trace(Visitor* visitor) const {
  visitor->Trace(search_params_);
  ScriptWrappable::Trace(visitor);
}

// static
URL* URL::parse(const String& str) {
  KURL url(str);
  if (!url.IsValid()) {
    return nullptr;
  }
  return MakeGarbageCollected<URL>(PassKey(), url);
}

// static
URL* URL::parse(const String& str, const String& base) {
  KURL base_url(base);
  if (!base_url.IsValid()) {
    return nullptr;
  }
  KURL url(base_url, str);
  if (!url.IsValid()) {
    return nullptr;
  }
  return MakeGarbageCollected<URL>(PassKey(), url);
}

// static
bool URL::canParse(const String& url) {
  return KURL(NullUrl(), url).IsValid();
}

// static
bool URL::canParse(const String& url, const String& base) {
  KURL base_url(base);
  return base_url.IsValid() && KURL(base_url, url).IsValid();
}

void URL::setHref(const String& value, ExceptionState& exception_state) {
  KURL url(value);
  if (!url.IsValid()) {
    exception_state.ThrowTypeError("Invalid URL");
    return;
  }
  url_ = url;
  Update();
}

void URL::setSearch(const String& value) {
  UrlUtils::setSearch(value);
  if (value.starts_with('?')) {
    UpdateSearchParams(value.substr(1));
  } else {
    UpdateSearchParams(value);
  }
}

String URL::CreatePublicURL(ExecutionContext* execution_context, Blob* blob) {
  return execution_context->GetPublicURLManager().RegisterUrl(blob);
}

String URL::CreatePublicURL(ExecutionContext* execution_context,
                            scoped_refptr<MediaSourceAttachment> attachment) {
  return execution_context->GetPublicURLManager().RegisterUrl(
      std::move(attachment));
}

URLSearchParams* URL::searchParams() {
  if (!search_params_) {
    search_params_ = URLSearchParams::Create(Url().Query().ToString(), this);
  }

  return search_params_.Get();
}

void URL::Update() {
  UpdateSearchParams(Url().Query().ToString());
}

void URL::UpdateSearchParams(const String& query_string) {
  if (!search_params_) {
    return;
  }

  base::AutoReset<bool> scope(&is_in_update_, true);
#if DCHECK_IS_ON()
  DCHECK_EQ(search_params_->UrlObject(), this);
#endif
  search_params_->SetInputWithoutUpdate(query_string);
}

}  // namespace blink
