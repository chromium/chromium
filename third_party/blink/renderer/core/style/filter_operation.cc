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

#include "third_party/blink/renderer/core/style/filter_operation.h"

#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/animation/animation_utilities.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_drop_shadow.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_gaussian_blur.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"

namespace blink {

FilterOperation* FilterOperation::Blend(const FilterOperation* from,
                                        const FilterOperation* to,
                                        double progress) {
  DCHECK(from || to);
  if (to)
    return to->Blend(from, progress);
  return from->Blend(nullptr, 1 - progress);
}

void ReferenceFilterOperation::Trace(blink::Visitor* visitor) {
  visitor->Trace(resource_);
  visitor->Trace(filter_);
  FilterOperation::Trace(visitor);
}

FloatRect ReferenceFilterOperation::MapRect(const FloatRect& rect) const {
  const auto* last_effect = filter_ ? filter_->LastEffect() : nullptr;
  if (!last_effect)
    return rect;
  return last_effect->MapRect(rect);
}

ReferenceFilterOperation::ReferenceFilterOperation(const AtomicString& url,
                                                   SVGResource* resource)
    : FilterOperation(REFERENCE), url_(url), resource_(resource) {}

void ReferenceFilterOperation::AddClient(SVGResourceClient& client) {
  if (resource_)
    resource_->AddClient(client);
}

void ReferenceFilterOperation::RemoveClient(SVGResourceClient& client) {
  if (resource_)
    resource_->RemoveClient(client);
}

bool ReferenceFilterOperation::operator==(const FilterOperation& o) const {
  if (!IsSameType(o))
    return false;
  const auto& other = To<ReferenceFilterOperation>(o);
  return url_ == other.url_ && resource_ == other.resource_;
}

FilterOperation* BasicColorMatrixFilterOperation::Blend(
    const FilterOperation* from,
    double progress) const {
  double from_amount;
  if (from) {
    SECURITY_DCHECK(from->IsSameType(*this));
    from_amount = To<BasicColorMatrixFilterOperation>(from)->Amount();
  } else {
    switch (type_) {
      case GRAYSCALE:
      case SEPIA:
      case HUE_ROTATE:
        from_amount = 0;
        break;
      case SATURATE:
        from_amount = 1;
        break;
      default:
        from_amount = 0;
        NOTREACHED();
    }
  }

  double result = blink::Blend(from_amount, amount_, progress);
  switch (type_) {
    case HUE_ROTATE:
      break;
    case GRAYSCALE:
    case SEPIA:
      result = clampTo<double>(result, 0, 1);
      break;
    case SATURATE:
      result = clampTo<double>(result, 0);
      break;
    default:
      NOTREACHED();
  }
  return MakeGarbageCollected<BasicColorMatrixFilterOperation>(result, type_);
}

FilterOperation* BasicComponentTransferFilterOperation::Blend(
    const FilterOperation* from,
    double progress) const {
  double from_amount;
  if (from) {
    SECURITY_DCHECK(from->IsSameType(*this));
    from_amount = To<BasicComponentTransferFilterOperation>(from)->Amount();
  } else {
    switch (type_) {
      case OPACITY:
      case CONTRAST:
      case BRIGHTNESS:
        from_amount = 1;
        break;
      case INVERT:
        from_amount = 0;
        break;
      default:
        from_amount = 0;
        NOTREACHED();
    }
  }

  double result = blink::Blend(from_amount, amount_, progress);
  switch (type_) {
    case BRIGHTNESS:
    case CONTRAST:
      result = clampTo<double>(result, 0);
      break;
    case INVERT:
    case OPACITY:
      result = clampTo<double>(result, 0, 1);
      break;
    default:
      NOTREACHED();
  }
  return MakeGarbageCollected<BasicComponentTransferFilterOperation>(result,
                                                                     type_);
}

FloatRect BlurFilterOperation::MapRect(const FloatRect& rect) const {
  float std_deviation = FloatValueForLength(std_deviation_, 0);
  return FEGaussianBlur::MapEffect(FloatSize(std_deviation, std_deviation),
                                   rect);
}

FilterOperation* BlurFilterOperation::Blend(const FilterOperation* from,
                                            double progress) const {
  Length::Type length_type = std_deviation_.GetType();
  if (!from)
    return MakeGarbageCollected<BlurFilterOperation>(std_deviation_.Blend(
        Length(length_type), progress, kValueRangeNonNegative));

  const auto* from_op = To<BlurFilterOperation>(from);
  return MakeGarbageCollected<BlurFilterOperation>(std_deviation_.Blend(
      from_op->std_deviation_, progress, kValueRangeNonNegative));
}

FloatRect DropShadowFilterOperation::MapRect(const FloatRect& rect) const {
  float std_deviation = shadow_.Blur();
  return FEDropShadow::MapEffect(FloatSize(std_deviation, std_deviation),
                                 shadow_.Location(), rect);
}

FilterOperation* DropShadowFilterOperation::Blend(const FilterOperation* from,
                                                  double progress) const {
  if (!from) {
    return Create(shadow_.Blend(ShadowData::NeutralValue(), progress,
                                Color::kTransparent));
  }

  const auto& from_op = To<DropShadowFilterOperation>(*from);
  return Create(shadow_.Blend(from_op.shadow_, progress, Color::kTransparent));
}

FloatRect BoxReflectFilterOperation::MapRect(const FloatRect& rect) const {
  return reflection_.MapRect(rect);
}

FilterOperation* BoxReflectFilterOperation::Blend(const FilterOperation* from,
                                                  double progress) const {
  NOTREACHED();
  return nullptr;
}

bool BoxReflectFilterOperation::operator==(const FilterOperation& o) const {
  if (!IsSameType(o))
    return false;
  const auto& other = static_cast<const BoxReflectFilterOperation&>(o);
  return reflection_ == other.reflection_;
}

}  // namespace blink
