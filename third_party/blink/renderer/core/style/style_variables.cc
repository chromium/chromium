// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_variables.h"

#include "base/memory/values_equivalent.h"

namespace blink {

bool StyleVariables::operator==(const StyleVariables& other) const {
  if (data_hash_ != other.data_hash_ || values_hash_ != other.values_hash_) {
    return false;
  }

  // NOTE: If two roots are equal but not the same, we set them
  // to be the same (we arbitrarily pick the one with the lowest
  // pointer value, so that we know we'll never flip-flop between
  // three or more). We could have done this on HashTrieNode
  // to get partial deduplication, but it doesn't seem to be worth it.

  if (data_root_ != other.data_root_) {
    if (*data_root_ == *other.data_root_) {
      data_root_ = other.data_root_ = std::min(data_root_, other.data_root_);
      data_root_->MakeShared();
    } else {
      return false;
    }
  }

  if (values_root_ != other.values_root_) {
    if (*values_root_ == *other.values_root_) {
      values_root_ = other.values_root_ =
          std::min(values_root_, other.values_root_);
      values_root_->MakeShared();
    } else {
      return false;
    }
  }

  return true;
}

void StyleVariables::SetData(const AtomicString& name, CSSVariableData* data) {
  data_root_ = data_root_->Set(name, data, data_hash_);
}

void StyleVariables::SetValue(const AtomicString& name, const CSSValue* value) {
  values_root_ = values_root_->Set(name, value, values_hash_);
}

bool StyleVariables::IsEmpty() const {
  return data_hash_ == 0 && values_hash_ == 0 && data_root_->empty() &&
         values_root_->empty();
}

void StyleVariables::CollectNames(HashSet<AtomicString>& names) const {
  data_root_->CollectNames(names);
}

std::ostream& operator<<(std::ostream& stream,
                         const StyleVariables& variables) {
  stream << "[";
  variables.data_root_->Serialize(
      [](const CSSVariableData* data) {
        return data ? data->Serialize() : "(null)";
      },
      stream);
  stream << "][";
  variables.values_root_->Serialize(
      [](const CSSValue* value) { return value ? value->CssText() : "(null)"; },
      stream);
  return stream << "]";
}

}  // namespace blink
