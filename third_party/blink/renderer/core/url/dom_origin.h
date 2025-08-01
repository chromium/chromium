// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_DOM_ORIGIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_DOM_ORIGIN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT DOMOrigin final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Creates a unique opaque origin:
  static DOMOrigin* Create();

  // Parses |value|, throwing an error if it isn't a validly serialized origin:
  static DOMOrigin* Create(const String& value,
                           ExceptionState& exception_state);

  explicit DOMOrigin(scoped_refptr<const SecurityOrigin> origin);
  ~DOMOrigin() override;

  // Parses |value|, returning `null` if it isn't a validly serialized origin:
  static DOMOrigin* parse(const String& value);

  // Parses |value|, returning `null` if it isn't a validly serialized URL:
  static DOMOrigin* fromURL(const String& value);

  String toJSON() const;

  bool opaque() const;

  bool isSameOrigin(const DOMOrigin* other) const;
  bool isSameSite(const DOMOrigin* other) const;

  void Trace(Visitor*) const override;

 private:
  const scoped_refptr<const SecurityOrigin> origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_DOM_ORIGIN_H_
