// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_VALUE_H_

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Represents the components of a PropertySpecificKeyframe's value that change
// smoothly as it interpolates to an adjacent value.
class CORE_EXPORT InterpolableValue {
  USING_FAST_MALLOC(InterpolableValue);

 public:
  virtual ~InterpolableValue() = default;

  // Interpolates from |this| InterpolableValue towards |to| at the given
  // |progress|, placing the output in |result|. That is:
  //
  //   result = this * (1 - progress) + to * progress
  //
  // Callers must make sure that |this|, |to|, and |result| are all of the same
  // concrete subclass.
  virtual void Interpolate(const InterpolableValue& to,
                           const double progress,
                           InterpolableValue& result) const = 0;

  virtual bool IsNumber() const { return false; }
  virtual bool IsBool() const { return false; }
  virtual bool IsList() const { return false; }
  virtual bool IsLength() const { return false; }
  virtual bool IsShadow() const { return false; }
  virtual bool IsFilter() const { return false; }
  virtual bool IsTransformList() const { return false; }

  // TODO(alancutter): Remove Equals().
  virtual bool Equals(const InterpolableValue&) const = 0;
  virtual void Scale(double scale) = 0;
  virtual void Add(const InterpolableValue& other) = 0;
  // The default implementation should be sufficient for most types, but
  // subclasses can override this to be more efficient if they chose.
  virtual void ScaleAndAdd(double scale, const InterpolableValue& other) {
    Scale(scale);
    Add(other);
  }
  virtual void AssertCanInterpolateWith(
      const InterpolableValue& other) const = 0;

  // Clone this value, optionally zeroing out the components at the same time.
  // These are not virtual to allow for covariant return types; see
  // documentation on RawClone/RawCloneAndZero.
  std::unique_ptr<InterpolableValue> Clone() const {
    return std::unique_ptr<InterpolableValue>(RawClone());
  }
  std::unique_ptr<InterpolableValue> CloneAndZero() const {
    return std::unique_ptr<InterpolableValue>(RawCloneAndZero());
  }

 private:
  // Helper methods to allow covariant Clone/CloneAndZero methods. Concrete
  // subclasses should not expose these methods publically, but instead should
  // declare their own version of Clone/CloneAndZero with a concrete return type
  // if it is useful for their clients.
  virtual InterpolableValue* RawClone() const = 0;
  virtual InterpolableValue* RawCloneAndZero() const = 0;
};

class CORE_EXPORT InterpolableNumber final : public InterpolableValue {
 public:
  explicit InterpolableNumber(double value) : value_(value) {}

  double Value() const { return value_; }
  void Set(double value) { value_ = value; }

  // InterpolableValue
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  bool IsNumber() const final { return true; }
  bool Equals(const InterpolableValue& other) const final;
  void Scale(double scale) final;
  void Add(const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  InterpolableNumber* RawClone() const final {
    return new InterpolableNumber(value_);
  }
  InterpolableNumber* RawCloneAndZero() const final {
    return new InterpolableNumber(0);
  }

  double value_;
};

class CORE_EXPORT InterpolableList : public InterpolableValue {
 public:
  // Explicitly delete operator= because MSVC automatically generate
  // copy constructors and operator= for dll-exported classes.
  // Since InterpolableList is not copyable, automatically generated
  // operator= causes MSVC compiler error.
  // However, we cannot use DISALLOW_COPY_AND_ASSIGN because InterpolableList
  // has its own copy constructor. So just delete operator= here.
  InterpolableList& operator=(const InterpolableList&) = delete;

  explicit InterpolableList(wtf_size_t size) : values_(size) {}

  InterpolableList(const InterpolableList& other) : values_(other.length()) {
    for (wtf_size_t i = 0; i < length(); i++)
      Set(i, other.values_[i]->Clone());
  }

  const InterpolableValue* Get(wtf_size_t position) const {
    return values_[position].get();
  }
  std::unique_ptr<InterpolableValue>& GetMutable(wtf_size_t position) {
    return values_[position];
  }
  wtf_size_t length() const { return values_.size(); }
  void Set(wtf_size_t position, std::unique_ptr<InterpolableValue> value) {
    values_[position] = std::move(value);
  }

  // InterpolableValue
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  bool IsList() const final { return true; }
  bool Equals(const InterpolableValue& other) const final;
  void Scale(double scale) final;
  void Add(const InterpolableValue& other) final;
  // We override this to avoid two passes on the list from the base version.
  void ScaleAndAdd(double scale, const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  InterpolableList* RawClone() const final {
    return new InterpolableList(*this);
  }
  InterpolableList* RawCloneAndZero() const final;

  Vector<std::unique_ptr<InterpolableValue>> values_;
};

DEFINE_TYPE_CASTS(InterpolableNumber,
                  InterpolableValue,
                  value,
                  value->IsNumber(),
                  value.IsNumber());
DEFINE_TYPE_CASTS(InterpolableList,
                  InterpolableValue,
                  value,
                  value->IsList(),
                  value.IsList());

}  // namespace blink

#endif
