/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CROSSFADE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CROSSFADE_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_image_generator_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ImageResourceObserver;

namespace cssvalue {

class CORE_EXPORT CSSCrossfadeValue final : public CSSImageGeneratorValue {
 public:
  CSSCrossfadeValue(CSSValue* from_value,
                    CSSValue* to_value,
                    CSSPrimitiveValue* percentage_value);
  ~CSSCrossfadeValue();

  CSSValue& From() const { return *from_value_; }
  CSSValue& To() const { return *to_value_; }
  CSSPrimitiveValue& Percentage() const { return *percentage_value_; }

  bool HasClients() const { return !Clients().empty(); }
  ImageResourceObserver* GetObserverProxy();

  String CustomCSSText() const;
  bool HasFailedOrCanceledSubresources() const;
  bool Equals(const CSSCrossfadeValue&) const;

  void TraceAfterDispatch(Visitor*) const;

 private:
  class ObserverProxy;

  Member<CSSValue> from_value_;
  Member<CSSValue> to_value_;
  Member<CSSPrimitiveValue> percentage_value_;
  Member<ObserverProxy> observer_proxy_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSCrossfadeValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsCrossfadeValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CROSSFADE_VALUE_H_
