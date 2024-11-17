// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_uri_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace cssvalue {

CSSURIValue::CSSURIValue(CSSUrlData url_data)
    : CSSValue(kURIClass), url_data_(std::move(url_data)) {}

CSSURIValue::~CSSURIValue() = default;

SVGResource* CSSURIValue::EnsureResourceReference() const {
  if (!resource_) {
    resource_ =
        MakeGarbageCollected<ExternalSVGResourceDocumentContent>(AbsoluteUrl());
  }
  return resource_.Get();
}

void CSSURIValue::ReResolveUrl(const Document& document) const {
  if (url_data_.ReResolveUrl(document)) {
    resource_ = nullptr;
  }
}

String CSSURIValue::CustomCSSText() const {
  return url_data_.CssText();
}

AtomicString CSSURIValue::FragmentIdentifier() const {
  // Always use KURL's FragmentIdentifier to ensure that we're handling the
  // fragment in a consistent manner.
  return AbsoluteUrl().FragmentIdentifier().ToAtomicString();
}

const AtomicString& CSSURIValue::NormalizedFragmentIdentifier() const {
  if (normalized_fragment_identifier_cache_.IsNull()) {
    normalized_fragment_identifier_cache_ =
        AtomicString(DecodeURLEscapeSequences(
            FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic));
  }

  // NOTE: If is_local_ is true, the normalized URL may be different
  // (we don't invalidate the cache when the base URL changes),
  // but it should not matter for the fragment. We DCHECK that we get
  // the right result, to be sure.
  DCHECK_EQ(normalized_fragment_identifier_cache_,
            AtomicString(DecodeURLEscapeSequences(
                FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic)));

  return normalized_fragment_identifier_cache_;
}

KURL CSSURIValue::AbsoluteUrl() const {
  return KURL(url_data_.ResolvedUrl());
}

bool CSSURIValue::IsLocal(const Document& document) const {
  return url_data_.IsLocal(document);
}

bool CSSURIValue::Equals(const CSSURIValue& other) const {
  return url_data_ == other.url_data_;
}

CSSURIValue* CSSURIValue::ComputedCSSValue(
    const KURL& base_url,
    const WTF::TextEncoding& charset) const {
  return MakeGarbageCollected<CSSURIValue>(
      url_data_.MakeResolved(base_url, charset));
}

void CSSURIValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(resource_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
