// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REGISTRY_ASSIGNMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REGISTRY_ASSIGNMENT_H_

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/stack_allocated.h"

namespace blink {

class CustomElementRegistry;

// Describes how a tree scope or element resolves its custom element registry:
//
//   * Inherit          -> resolve to the document's global registry (the
//                         default; nothing is stored).
//   * Wait             -> the registry is explicitly null and waiting for a
//                         scoped registry to be assigned later.
//   * Explicit(reg)    -> use `reg` (a scoped or global registry).
class CustomElementRegistryAssignment {
  STACK_ALLOCATED();

 public:
  enum class NullRegistryFallback {
    // A null registry is an intentional waiting state.
    kWait,
    // A null registry means there is no override, so inherit from tree scope.
    kInherit,
  };

  static CustomElementRegistryAssignment Inherit() {
    return CustomElementRegistryAssignment(Type::kInherit, nullptr);
  }
  static CustomElementRegistryAssignment Wait() {
    return CustomElementRegistryAssignment(Type::kWait, nullptr);
  }
  static CustomElementRegistryAssignment Explicit(
      CustomElementRegistry* registry) {
    DCHECK(registry);
    return CustomElementRegistryAssignment(Type::kExplicit, registry);
  }
  static CustomElementRegistryAssignment ResolveNullableRegistry(
      CustomElementRegistry* registry,
      NullRegistryFallback fallback) {
    if (registry) {
      return Explicit(registry);
    }

    switch (fallback) {
      case NullRegistryFallback::kWait:
        return Wait();
      case NullRegistryFallback::kInherit:
        return Inherit();
    }
  }

  bool IsInherit() const { return type_ == Type::kInherit; }
  bool IsWait() const { return type_ == Type::kWait; }
  bool IsExplicit() const { return type_ == Type::kExplicit; }

  // Only valid when IsExplicit(); always non-null in that case.
  CustomElementRegistry* Registry() const {
    DCHECK(IsExplicit());
    return registry_;
  }

 private:
  enum class Type { kInherit, kWait, kExplicit };

  CustomElementRegistryAssignment(Type type, CustomElementRegistry* registry)
      : type_(type), registry_(registry) {
    DCHECK_EQ(type == Type::kExplicit, registry != nullptr);
  }

  Type type_;
  CustomElementRegistry* registry_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REGISTRY_ASSIGNMENT_H_
