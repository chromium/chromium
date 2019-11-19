// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URI_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URI_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class KURL;
class SVGResource;

namespace cssvalue {

class CORE_EXPORT CSSURIValue : public CSSValue {
 public:
  static CSSURIValue* Create(const String& relative_url, const KURL& url) {
    return MakeGarbageCollected<CSSURIValue>(AtomicString(relative_url), url);
  }
  static CSSURIValue* Create(const AtomicString& absolute_url) {
    return MakeGarbageCollected<CSSURIValue>(absolute_url, absolute_url);
  }

  CSSURIValue(const AtomicString&, const KURL&);
  CSSURIValue(const AtomicString& relative_url,
              const AtomicString& absolute_url);
  ~CSSURIValue();

  SVGResource* EnsureResourceReference() const;
  void ReResolveUrl(const Document&) const;

  const AtomicString& ValueForSerialization() const {
    return is_local_ ? relative_url_ : absolute_url_;
  }

  String CustomCSSText() const;

  bool IsLocal(const Document&) const;
  AtomicString FragmentIdentifier() const;

  bool Equals(const CSSURIValue&) const;

  CSSURIValue* ValueWithURLMadeAbsolute(const KURL& base_url,
                                        const WTF::TextEncoding&) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  KURL AbsoluteUrl() const;

  AtomicString relative_url_;
  bool is_local_;

  mutable Member<SVGResource> resource_;
  mutable AtomicString absolute_url_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSURIValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsURIValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URI_VALUE_H_
