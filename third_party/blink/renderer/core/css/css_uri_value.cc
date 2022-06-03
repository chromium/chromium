// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_uri_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace cssvalue {

CSSURIValue::CSSURIValue(const AtomicString& relative_url,
                         const AtomicString& absolute_url)
    : CSSValue(kURIClass),
      relative_url_(relative_url),
      is_local_(relative_url.StartsWith('#')),
      absolute_url_(absolute_url) {}

CSSURIValue::CSSURIValue(const AtomicString& absolute_url)
    : CSSURIValue(absolute_url, absolute_url) {}

CSSURIValue::CSSURIValue(const AtomicString& relative_url, const KURL& url)
    : CSSURIValue(relative_url, AtomicString(url.GetString())) {}

CSSURIValue::~CSSURIValue() = default;

SVGResource* CSSURIValue::EnsureResourceReference() const {
  if (!resource_)
    resource_ = MakeGarbageCollected<ExternalSVGResource>(AbsoluteUrl());
  return resource_;
}

void CSSURIValue::ReResolveUrl(const Document& document) const {
  KURL url = document.CompleteURL(relative_url_);
  AtomicString url_string(url.GetString());
  if (url_string == absolute_url_)
    return;
  absolute_url_ = url_string;
  resource_ = nullptr;
}

String CSSURIValue::CustomCSSText() const {
  return SerializeURI(relative_url_);
}

AtomicString CSSURIValue::FragmentIdentifier() const {
  // Always use KURL's FragmentIdentifier to ensure that we're handling the
  // fragment in a consistent manner.
  return AtomicString(AbsoluteUrl().FragmentIdentifier());
}

KURL CSSURIValue::AbsoluteUrl() const {
  return KURL(absolute_url_);
}

bool CSSURIValue::IsLocal(const Document& document) const {
  return is_local_ ||
         EqualIgnoringFragmentIdentifier(AbsoluteUrl(), document.Url());
}

bool CSSURIValue::Equals(const CSSURIValue& other) const {
  // If only one has the 'local url' flag set, the URLs can't match.
  if (is_local_ != other.is_local_)
    return false;
  if (is_local_)
    return relative_url_ == other.relative_url_;
  return absolute_url_ == other.absolute_url_;
}

CSSURIValue* CSSURIValue::ValueWithURLMadeAbsolute(
    const KURL& base_url,
    const WTF::TextEncoding& charset) const {
  if (!charset.IsValid()) {
    return MakeGarbageCollected<CSSURIValue>(
        AtomicString(KURL(base_url, relative_url_).GetString()));
  }
  return MakeGarbageCollected<CSSURIValue>(
      AtomicString(KURL(base_url, relative_url_, charset).GetString()));
}

void CSSURIValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(resource_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
