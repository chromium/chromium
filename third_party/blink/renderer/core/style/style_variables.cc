// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_variables.h"

#include "third_party/blink/renderer/core/style/data_equivalency.h"

namespace blink {

namespace {

using OptionalData = StyleVariables::OptionalData;
using OptionalValue = StyleVariables::OptionalValue;

bool IsEqual(const OptionalData& a, const OptionalData& b) {
  if (a.has_value() != b.has_value())
    return false;
  if (!a.has_value())
    return true;
  return DataEquivalent(a.value(), b.value());
}

bool IsEqual(const OptionalValue& a, const OptionalValue& b) {
  if (a.has_value() != b.has_value())
    return false;
  if (!a.has_value())
    return true;
  return DataEquivalent(a.value(), b.value());
}

}  // namespace

StyleVariables::StyleVariables() : values_(MakeGarbageCollected<ValueMap>()) {}

StyleVariables::StyleVariables(const StyleVariables& other)
    : data_(other.data_),
      values_(MakeGarbageCollected<ValueMap>(*other.values_)) {}

bool StyleVariables::operator==(const StyleVariables& other) const {
  if (data_.size() != other.data_.size())
    return false;

  for (const auto& pair : data_) {
    if (!IsEqual(GetData(pair.key), other.GetData(pair.key)))
      return false;
  }

  if (values_->size() != other.values_->size())
    return false;

  for (const auto& pair : *values_) {
    if (!IsEqual(GetValue(pair.key), other.GetValue(pair.key)))
      return false;
  }

  return true;
}

StyleVariables::OptionalData StyleVariables::GetData(
    const AtomicString& name) const {
  auto i = data_.find(name);
  if (i != data_.end())
    return i->value.get();
  return base::nullopt;
}

StyleVariables::OptionalValue StyleVariables::GetValue(
    const AtomicString& name) const {
  auto i = values_->find(name);
  if (i != values_->end())
    return i->value;
  return base::nullopt;
}

void StyleVariables::SetData(const AtomicString& name,
                             scoped_refptr<CSSVariableData> data) {
  data_.Set(name, std::move(data));
}

void StyleVariables::SetValue(const AtomicString& name, const CSSValue* value) {
  values_->Set(name, value);
}

bool StyleVariables::IsEmpty() const {
  return data_.IsEmpty() && values_->IsEmpty();
}

HashSet<AtomicString> StyleVariables::GetNames() const {
  HashSet<AtomicString> names;
  for (const auto& pair : data_)
    names.insert(pair.key);
  return names;
}

}  // namespace blink
