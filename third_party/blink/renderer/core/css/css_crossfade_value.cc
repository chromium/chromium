/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_crossfade_value.h"

#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSCrossfadeValue::CSSCrossfadeValue(CSSValue* from_value,
                                     CSSValue* to_value,
                                     CSSPrimitiveValue* percentage_value)
    : CSSImageGeneratorValue(kCrossfadeClass),
      from_value_(from_value),
      to_value_(to_value),
      percentage_value_(percentage_value) {}

CSSCrossfadeValue::~CSSCrossfadeValue() = default;

String CSSCrossfadeValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("-webkit-cross-fade(");
  result.Append(from_value_->CssText());
  result.Append(", ");
  result.Append(to_value_->CssText());
  result.Append(", ");
  result.Append(percentage_value_->CssText());
  result.Append(')');
  return result.ReleaseString();
}

bool CSSCrossfadeValue::HasFailedOrCanceledSubresources() const {
  return from_value_->HasFailedOrCanceledSubresources() ||
         to_value_->HasFailedOrCanceledSubresources();
}

bool CSSCrossfadeValue::Equals(const CSSCrossfadeValue& other) const {
  return base::ValuesEquivalent(from_value_, other.from_value_) &&
         base::ValuesEquivalent(to_value_, other.to_value_) &&
         base::ValuesEquivalent(percentage_value_, other.percentage_value_);
}

class CSSCrossfadeValue::ObserverProxy final
    : public GarbageCollected<CSSCrossfadeValue::ObserverProxy>,
      public ImageResourceObserver {
 public:
  explicit ObserverProxy(CSSCrossfadeValue* owner) : owner_(owner) {}

  void ImageChanged(ImageResourceContent*,
                    CanDeferInvalidation defer) override {
    for (const ImageResourceObserver* const_observer : Clients().Keys()) {
      auto* observer = const_cast<ImageResourceObserver*>(const_observer);
      observer->ImageChanged(static_cast<WrappedImagePtr>(owner_), defer);
    }
  }

  bool WillRenderImage() override {
    for (const ImageResourceObserver* const_observer : Clients().Keys()) {
      auto* observer = const_cast<ImageResourceObserver*>(const_observer);
      if (observer->WillRenderImage())
        return true;
    }
    return false;
  }

  bool GetImageAnimationPolicy(
      mojom::blink::ImageAnimationPolicy& animation_policy) override {
    for (const ImageResourceObserver* const_observer : Clients().Keys()) {
      auto* observer = const_cast<ImageResourceObserver*>(const_observer);
      if (observer->GetImageAnimationPolicy(animation_policy))
        return true;
    }
    return false;
  }

  String DebugName() const override { return "CrossfadeObserverProxy"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(owner_);
    ImageResourceObserver::Trace(visitor);
  }

 private:
  const ClientSizeCountMap& Clients() const { return owner_->Clients(); }

  Member<CSSCrossfadeValue> owner_;
};

ImageResourceObserver* CSSCrossfadeValue::GetObserverProxy() {
  if (!observer_proxy_)
    observer_proxy_ = MakeGarbageCollected<ObserverProxy>(this);
  return observer_proxy_;
}

void CSSCrossfadeValue::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(from_value_);
  visitor->Trace(to_value_);
  visitor->Trace(percentage_value_);
  visitor->Trace(observer_proxy_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
