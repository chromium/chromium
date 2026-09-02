// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_RARE_DATA_UPDATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_RARE_DATA_UPDATE_H_

#include <concepts>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/stack_allocated.h"
#include "base/types/pass_key.h"

namespace blink {

class Node;
class NodeRareData;

// A move-only, stack-allocated wrapper returned by NodeRareData::Ensure*
// methods. To obtain the reference to `T&`, callers must invoke
// `RefreshNodeAndUnwrap(node)`, which synchronizes the owning Node's rare data
// pointer.
//
// This indirection exists because `NodeRareData` itself does not have a pointer
// to its owning `Node`, but ensuring space for a new field might need to grow
// the backing store.
template <typename T>
class [[nodiscard]] RareDataUpdate final {
  STACK_ALLOCATED();

  static_assert(
      !std::is_reference_v<T>,
      "RareDataUpdate<T> template parameter must not be a reference.");
  static_assert(!std::is_pointer_v<T>,
                "RareDataUpdate<T> template parameter must not be a pointer.");

 public:
  RareDataUpdate(const RareDataUpdate&) = delete;
  RareDataUpdate& operator=(const RareDataUpdate&) = delete;
  RareDataUpdate(RareDataUpdate&& other) noexcept
      : field_(std::exchange(other.field_, nullptr)),
        rare_data_(std::exchange(other.rare_data_, nullptr)) {}
  RareDataUpdate& operator=(RareDataUpdate&&) = delete;
  ~RareDataUpdate() = default;

  template <typename U>
    requires std::derived_from<U, T>
  RareDataUpdate(RareDataUpdate<U>&& other) noexcept
      : field_(std::exchange(other.field_, nullptr)),
        rare_data_(std::exchange(other.rare_data_, nullptr)) {}

  // Consumes the update, refreshes node.data_, and returns the field reference.
  ALWAYS_INLINE T& RefreshNodeAndUnwrap(Node& node) &&;

 private:
  template <typename>
  friend class RareDataUpdate;
  friend class NodeRareData;

  ALWAYS_INLINE RareDataUpdate(base::PassKey<NodeRareData>,
                               T& field,
                               NodeRareData* rare_data)
      : field_(&field), rare_data_(rare_data) {}

  T* field_ = nullptr;
  NodeRareData* rare_data_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_RARE_DATA_UPDATE_H_
