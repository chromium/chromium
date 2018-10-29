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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILTER_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILTER_OPERATION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/box_reflection.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Filter;
class SVGResource;
class SVGResourceClient;

// CSS Filters

class CORE_EXPORT FilterOperation
    : public GarbageCollectedFinalized<FilterOperation> {

 public:
  enum OperationType {
    REFERENCE,  // url(#somefilter)
    GRAYSCALE,
    SEPIA,
    SATURATE,
    HUE_ROTATE,
    INVERT,
    OPACITY,
    BRIGHTNESS,
    CONTRAST,
    BLUR,
    DROP_SHADOW,
    BOX_REFLECT,
    NONE
  };

  static bool CanInterpolate(FilterOperation::OperationType type) {
    switch (type) {
      case GRAYSCALE:
      case SEPIA:
      case SATURATE:
      case HUE_ROTATE:
      case INVERT:
      case OPACITY:
      case BRIGHTNESS:
      case CONTRAST:
      case BLUR:
      case DROP_SHADOW:
        return true;
      case REFERENCE:
      case BOX_REFLECT:
        return false;
      case NONE:
        break;
    }
    NOTREACHED();
    return false;
  }

  virtual ~FilterOperation() = default;
  virtual void Trace(blink::Visitor* visitor) {}

  static FilterOperation* Blend(const FilterOperation* from,
                                const FilterOperation* to,
                                double progress);
  virtual bool operator==(const FilterOperation&) const = 0;
  bool operator!=(const FilterOperation& o) const { return !(*this == o); }

  OperationType GetType() const { return type_; }
  virtual bool IsSameType(const FilterOperation& o) const {
    return o.GetType() == type_;
  }

  // True if the alpha channel of any pixel can change under this operation.
  virtual bool AffectsOpacity() const { return false; }
  // True if the the value of one pixel can affect the value of another pixel
  // under this operation, such as blur.
  virtual bool MovesPixels() const { return false; }

  // Maps "forward" to determine which pixels in a destination rect are
  // affected by pixels in the source rect.
  // See also FilterEffect::MapRect.
  virtual FloatRect MapRect(const FloatRect& rect) const { return rect; }

 protected:
  FilterOperation(OperationType type) : type_(type) {}

  OperationType type_;

 private:
  virtual FilterOperation* Blend(const FilterOperation* from,
                                 double progress) const = 0;
  DISALLOW_COPY_AND_ASSIGN(FilterOperation);
};

#define DEFINE_FILTER_OPERATION_TYPE_CASTS(thisType, operationType)  \
  DEFINE_TYPE_CASTS(thisType, FilterOperation, op,                   \
                    op->GetType() == FilterOperation::operationType, \
                    op.GetType() == FilterOperation::operationType);

class CORE_EXPORT ReferenceFilterOperation : public FilterOperation {
 public:
  static ReferenceFilterOperation* Create(const AtomicString& url,
                                          SVGResource* resource) {
    return new ReferenceFilterOperation(url, resource);
  }

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

  const AtomicString& Url() const { return url_; }

  Filter* GetFilter() const { return filter_.Get(); }
  void SetFilter(Filter* filter) { filter_ = filter; }

  SVGResource* Resource() const { return resource_; }

  void AddClient(SVGResourceClient&);
  void RemoveClient(SVGResourceClient&);

  void Trace(blink::Visitor*) override;

 private:
  ReferenceFilterOperation(const AtomicString& url, SVGResource*);

  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override {
    NOTREACHED();
    return nullptr;
  }

  bool operator==(const FilterOperation&) const override;

  AtomicString url_;
  Member<SVGResource> resource_;
  Member<Filter> filter_;
};

DEFINE_FILTER_OPERATION_TYPE_CASTS(ReferenceFilterOperation, REFERENCE);

// GRAYSCALE, SEPIA, SATURATE and HUE_ROTATE are variations on a basic color
// matrix effect.  For HUE_ROTATE, the angle of rotation is stored in m_amount.
class CORE_EXPORT BasicColorMatrixFilterOperation : public FilterOperation {
 public:
  static BasicColorMatrixFilterOperation* Create(double amount,
                                                 OperationType type) {
    return new BasicColorMatrixFilterOperation(amount, type);
  }

  double Amount() const { return amount_; }

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const BasicColorMatrixFilterOperation* other =
        static_cast<const BasicColorMatrixFilterOperation*>(&o);
    return amount_ == other->amount_;
  }

  BasicColorMatrixFilterOperation(double amount, OperationType type)
      : FilterOperation(type), amount_(amount) {}

  double amount_;
};

inline bool IsBasicColorMatrixFilterOperation(
    const FilterOperation& operation) {
  FilterOperation::OperationType type = operation.GetType();
  return type == FilterOperation::GRAYSCALE || type == FilterOperation::SEPIA ||
         type == FilterOperation::SATURATE ||
         type == FilterOperation::HUE_ROTATE;
}

DEFINE_TYPE_CASTS(BasicColorMatrixFilterOperation,
                  FilterOperation,
                  op,
                  IsBasicColorMatrixFilterOperation(*op),
                  IsBasicColorMatrixFilterOperation(op));

// INVERT, BRIGHTNESS, CONTRAST and OPACITY are variations on a basic component
// transfer effect.
class CORE_EXPORT BasicComponentTransferFilterOperation
    : public FilterOperation {
 public:
  static BasicComponentTransferFilterOperation* Create(double amount,
                                                       OperationType type) {
    return new BasicComponentTransferFilterOperation(amount, type);
  }

  double Amount() const { return amount_; }

  bool AffectsOpacity() const override { return type_ == OPACITY; }

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const BasicComponentTransferFilterOperation* other =
        static_cast<const BasicComponentTransferFilterOperation*>(&o);
    return amount_ == other->amount_;
  }

  BasicComponentTransferFilterOperation(double amount, OperationType type)
      : FilterOperation(type), amount_(amount) {}

  double amount_;
};

inline bool IsBasicComponentTransferFilterOperation(
    const FilterOperation& operation) {
  FilterOperation::OperationType type = operation.GetType();
  return type == FilterOperation::INVERT || type == FilterOperation::OPACITY ||
         type == FilterOperation::BRIGHTNESS ||
         type == FilterOperation::CONTRAST;
}

DEFINE_TYPE_CASTS(BasicComponentTransferFilterOperation,
                  FilterOperation,
                  op,
                  IsBasicComponentTransferFilterOperation(*op),
                  IsBasicComponentTransferFilterOperation(op));

class CORE_EXPORT BlurFilterOperation : public FilterOperation {
 public:
  static BlurFilterOperation* Create(const Length& std_deviation) {
    return new BlurFilterOperation(std_deviation);
  }

  const Length& StdDeviation() const { return std_deviation_; }

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const BlurFilterOperation* other =
        static_cast<const BlurFilterOperation*>(&o);
    return std_deviation_ == other->std_deviation_;
  }

  BlurFilterOperation(const Length& std_deviation)
      : FilterOperation(BLUR), std_deviation_(std_deviation) {}

  Length std_deviation_;
};

DEFINE_FILTER_OPERATION_TYPE_CASTS(BlurFilterOperation, BLUR);

class CORE_EXPORT DropShadowFilterOperation : public FilterOperation {
 public:
  static DropShadowFilterOperation* Create(const ShadowData& shadow) {
    return new DropShadowFilterOperation(shadow);
  }

  const ShadowData& Shadow() const { return shadow_; }

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const DropShadowFilterOperation* other =
        static_cast<const DropShadowFilterOperation*>(&o);
    return shadow_ == other->shadow_;
  }

  DropShadowFilterOperation(const ShadowData& shadow)
      : FilterOperation(DROP_SHADOW), shadow_(shadow) {}

  ShadowData shadow_;
};

DEFINE_FILTER_OPERATION_TYPE_CASTS(DropShadowFilterOperation, DROP_SHADOW);

class CORE_EXPORT BoxReflectFilterOperation : public FilterOperation {
 public:
  static BoxReflectFilterOperation* Create(const BoxReflection& reflection) {
    return new BoxReflectFilterOperation(reflection);
  }

  const BoxReflection& Reflection() const { return reflection_; }

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation&) const override;

  BoxReflectFilterOperation(const BoxReflection& reflection)
      : FilterOperation(BOX_REFLECT), reflection_(reflection) {}

  BoxReflection reflection_;
};
DEFINE_FILTER_OPERATION_TYPE_CASTS(BoxReflectFilterOperation, BOX_REFLECT);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILTER_OPERATION_H_
