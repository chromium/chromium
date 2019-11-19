// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_FILTER_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"

namespace blink {

class CSSValue;
class StyleResolverState;

// Represents a blink::FilterOperation, converted into a form that can be
// interpolated from/to.
class CORE_EXPORT InterpolableFilter final : public InterpolableValue {
 public:
  InterpolableFilter(std::unique_ptr<InterpolableValue> value,
                     FilterOperation::OperationType type)
      : value_(std::move(value)), type_(type) {
    DCHECK(value_);
  }

  static std::unique_ptr<InterpolableFilter> MaybeCreate(const FilterOperation&,
                                                         double zoom);
  static std::unique_ptr<InterpolableFilter> MaybeConvertCSSValue(
      const CSSValue&);

  // Create an InterpolableFilter representing the 'initial value for
  // interpolation' for the given OperationType.
  static std::unique_ptr<InterpolableFilter> CreateInitialValue(
      FilterOperation::OperationType);

  FilterOperation::OperationType GetType() const { return type_; }

  // Convert this InterpolableFilter back into a FilterOperation class, usually
  // to be applied to the style after interpolating |this|.
  FilterOperation* CreateFilterOperation(const StyleResolverState&) const;

  // InterpolableValue implementation:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  bool IsFilter() const final { return true; }
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }
  void Scale(double scale) final { NOTREACHED(); }
  void Add(const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  InterpolableFilter* RawClone() const final {
    return new InterpolableFilter(value_->Clone(), type_);
  }
  InterpolableFilter* RawCloneAndZero() const final {
    return new InterpolableFilter(value_->CloneAndZero(), type_);
  }

  // Stores the interpolable data for the filter. The form varies depending on
  // the |type_|; see the implementation file for details of the mapping.
  std::unique_ptr<InterpolableValue> value_;

  FilterOperation::OperationType type_;
};

template <>
struct DowncastTraits<InterpolableFilter> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsFilter();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_FILTER_H_
