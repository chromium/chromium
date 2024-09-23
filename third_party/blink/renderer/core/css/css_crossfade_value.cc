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

CSSCrossfadeValue::CSSCrossfadeValue(
    bool is_prefixed_variant,
    HeapVector<std::pair<Member<CSSValue>, Member<CSSPrimitiveValue>>>
        image_and_percentages)
    : CSSImageGeneratorValue(kCrossfadeClass),
      is_prefixed_variant_(is_prefixed_variant),
      image_and_percentages_(std::move(image_and_percentages)) {}

CSSCrossfadeValue::~CSSCrossfadeValue() = default;

String CSSCrossfadeValue::CustomCSSText() const {
  StringBuilder result;
  if (is_prefixed_variant_) {
    CHECK_EQ(2u, image_and_percentages_.size());
    result.Append("-webkit-cross-fade(");
    result.Append(image_and_percentages_[0].first->CssText());
    result.Append(", ");
    result.Append(image_and_percentages_[1].first->CssText());
    result.Append(", ");
    result.Append(image_and_percentages_[1].second->CssText());
    result.Append(')');
    DCHECK_EQ(nullptr, image_and_percentages_[0].second);
  } else {
    result.Append("cross-fade(");
    bool first = true;
    for (const auto& [image, percentage] : image_and_percentages_) {
      if (!first) {
        result.Append(", ");
      }
      result.Append(image->CssText());
      if (percentage) {
        result.Append(' ');
        result.Append(percentage->CssText());
      }
      first = false;
    }
    result.Append(')');
  }
  return result.ReleaseString();
}

bool CSSCrossfadeValue::HasFailedOrCanceledSubresources() const {
  return std::any_of(
      image_and_percentages_.begin(), image_and_percentages_.end(),
      [](const auto& image_and_percent) {
        return image_and_percent.first->HasFailedOrCanceledSubresources();
      });
}

bool CSSCrossfadeValue::Equals(const CSSCrossfadeValue& other) const {
  if (image_and_percentages_.size() != other.image_and_percentages_.size()) {
    return false;
  }
  for (unsigned i = 0; i < image_and_percentages_.size(); ++i) {
    if (!base::ValuesEquivalent(image_and_percentages_[i].first,
                                other.image_and_percentages_[i].first)) {
      return false;
    }
    if (!base::ValuesEquivalent(image_and_percentages_[i].second,
                                other.image_and_percentages_[i].second)) {
      return false;
    }
  }
  return true;
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
      if (observer->WillRenderImage()) {
        return true;
      }
    }
    return false;
  }

  bool GetImageAnimationPolicy(
      mojom::blink::ImageAnimationPolicy& animation_policy) override {
    for (const ImageResourceObserver* const_observer : Clients().Keys()) {
      auto* observer = const_cast<ImageResourceObserver*>(const_observer);
      if (observer->GetImageAnimationPolicy(animation_policy)) {
        return true;
      }
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

  Member<const CSSCrossfadeValue> owner_;
};

ImageResourceObserver* CSSCrossfadeValue::GetObserverProxy() {
  if (!observer_proxy_) {
    observer_proxy_ = MakeGarbageCollected<ObserverProxy>(this);
  }
  return observer_proxy_.Get();
}

void CSSCrossfadeValue::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(image_and_percentages_);
  visitor->Trace(observer_proxy_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
