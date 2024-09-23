// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_variables.h"

#include "base/memory/values_equivalent.h"

namespace blink {

namespace {

using OptionalData = StyleVariables::OptionalData;
using OptionalValue = StyleVariables::OptionalValue;

bool IsEqual(const OptionalData& a, const OptionalData& b) {
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a.has_value()) {
    return true;
  }
  return base::ValuesEquivalent(a.value(), b.value());
}

bool IsEqual(const OptionalValue& a, const OptionalValue& b) {
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a.has_value()) {
    return true;
  }
  return base::ValuesEquivalent(a.value(), b.value());
}

}  // namespace

bool StyleVariables::operator==(const StyleVariables& other) const {
  if (data_.size() != other.data_.size() ||
      values_.size() != other.values_.size()) {
    return false;
  }

  if (equality_cache_partner_ == &other &&
      other.equality_cache_partner_ == this) {
    DCHECK_EQ(equality_cached_result_, other.equality_cached_result_);
    return equality_cached_result_;
  }

  equality_cache_partner_ = &other;
  other.equality_cache_partner_ = this;

  for (const auto& pair : data_) {
    if (!IsEqual(GetData(pair.key), other.GetData(pair.key))) {
      equality_cached_result_ = other.equality_cached_result_ = false;
      return false;
    }
  }

  for (const auto& pair : values_) {
    if (!IsEqual(GetValue(pair.key), other.GetValue(pair.key))) {
      equality_cached_result_ = other.equality_cached_result_ = false;
      return false;
    }
  }

  equality_cached_result_ = other.equality_cached_result_ = true;
  return true;
}

StyleVariables::OptionalData StyleVariables::GetData(
    const AtomicString& name) const {
  auto i = data_.find(name);
  if (i != data_.end()) {
    return i->value.Get();
  }
  return std::nullopt;
}

StyleVariables::OptionalValue StyleVariables::GetValue(
    const AtomicString& name) const {
  auto i = values_.find(name);
  if (i != values_.end()) {
    return i->value.Get();
  }
  return std::nullopt;
}

void StyleVariables::SetData(const AtomicString& name, CSSVariableData* data) {
  data_.Set(name, data);
  equality_cache_partner_ = nullptr;
}

void StyleVariables::SetValue(const AtomicString& name, const CSSValue* value) {
  values_.Set(name, value);
  equality_cache_partner_ = nullptr;
}

bool StyleVariables::IsEmpty() const {
  return data_.empty() && values_.empty();
}

void StyleVariables::CollectNames(HashSet<AtomicString>& names) const {
  for (const auto& pair : data_) {
    names.insert(pair.key);
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const StyleVariables& variables) {
  stream << "[";
  for (const auto& [key, value] : variables.data_) {
    stream << key << ": " << value->Serialize() << ", ";
  }
  stream << "][";
  for (const auto& [key, value] : variables.values_) {
    stream << key << ": " << value->CssText() << ", ";
  }
  return stream << "]";
}

}  // namespace blink
