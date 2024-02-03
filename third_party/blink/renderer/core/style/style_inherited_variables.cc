// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_inherited_variables.h"

#include "base/memory/values_equivalent.h"

#include <iostream>

namespace blink {

bool StyleInheritedVariables::HasEquivalentRoots(
    const StyleInheritedVariables& other) const {
  if (base::ValuesEquivalent(root_, other.root_)) {
    return true;
  }
  // A non-null root pointer can be semantically the same as
  // a null root pointer; normalize them and try comparing again.
  if (root_ == nullptr) {
    return other.root_->variables_ == other.variables_;
  } else if (other.root_ == nullptr) {
    return root_->variables_ == variables_;
  } else {
    return false;
  }
}

bool StyleInheritedVariables::operator==(
    const StyleInheritedVariables& other) const {
  return HasEquivalentRoots(other) && variables_ == other.variables_;
}

StyleInheritedVariables::StyleInheritedVariables() : root_(nullptr) {}

StyleInheritedVariables::StyleInheritedVariables(
    StyleInheritedVariables& other) {
  if (!other.root_) {
    root_ = &other;
  } else {
    variables_ = other.variables_;
    root_ = other.root_;
  }
}

StyleVariables::OptionalData StyleInheritedVariables::GetData(
    const AtomicString& name) const {
  if (auto data = variables_.GetData(name)) {
    return *data;
  }
  if (root_) {
    return root_->variables_.GetData(name);
  }
  return std::nullopt;
}

StyleVariables::OptionalValue StyleInheritedVariables::GetValue(
    const AtomicString& name) const {
  if (auto data = variables_.GetValue(name)) {
    return *data;
  }
  if (root_) {
    return root_->variables_.GetValue(name);
  }
  return std::nullopt;
}

void StyleInheritedVariables::CollectNames(HashSet<AtomicString>& names) const {
  if (root_) {
    for (const auto& pair : root_->Data()) {
      names.insert(pair.key);
    }
  }
  for (const auto& pair : Data()) {
    names.insert(pair.key);
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const StyleInheritedVariables& variables) {
  if (variables.root_) {
    stream << "root: <" << *variables.root_ << "> ";
  }
  return stream << variables.variables_;
}

}  // namespace blink
