// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_CLONING_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_CLONING_DATA_H_

#include "base/containers/enum_set.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_part_root_clone_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

enum class CloneOption {
  kIncludeDescendants,
  kIncludeShadowRoots,
  kPreserveDOMParts,

  // For `CloneOptionSet`.
  kMinValue = kIncludeDescendants,
  kMaxValue = kPreserveDOMParts,
};

using CloneOptionSet =
    base::EnumSet<CloneOption, CloneOption::kMinValue, CloneOption::kMaxValue>;

class CORE_EXPORT NodeCloningData final {
  STACK_ALLOCATED();

 public:
  NodeCloningData() : NodeCloningData({}) {}
  NodeCloningData(std::initializer_list<CloneOption> values)
      : clone_options_(values) {}
  NodeCloningData(const NodeCloningData&) = delete;
  NodeCloningData& operator=(const NodeCloningData&) = delete;
  ~NodeCloningData() {}

  bool Has(CloneOption option) const { return clone_options_.Has(option); }
  void Put(CloneOption option) { clone_options_.Put(option); }
  void PushPartRoot(PartRoot& clone) {
    cloned_part_root_stack_.push_back(&clone);
  }
  void PopPartRoot(ChildNodePart& expected_top_of_stack) {
    if (PartRootStackInvalid()) {
      return;
    }
    PartRoot& top_of_stack = CurrentPartRoot();
    if (&top_of_stack != &expected_top_of_stack) {
      // Mis-nested ChildNodeParts invalidate the clone entirely.
      cloned_part_root_stack_.clear();
      return;
    }

    cloned_part_root_stack_.pop_back();
  }
  bool PartRootStackInvalid() const { return cloned_part_root_stack_.empty(); }

  PartRoot& CurrentPartRoot() const {
    DCHECK(!PartRootStackInvalid());
    return *cloned_part_root_stack_.back();
  }

  void SetPartRootCloneOptions(PartRootCloneOptions* options) {
    if (options && options->hasAttributeValues()) {
      attribute_values_.clear();
      for (String value : options->attributeValues()) {
        attribute_values_.push_back(AtomicString(value));
      }
      current_attribute_index_ = 0;
    }
  }

  absl::optional<AtomicString> NextAttributeValue() {
    if (current_attribute_index_ >= attribute_values_.size()) {
      return absl::nullopt;
    }
    return attribute_values_[current_attribute_index_++];
  }

 private:
  CloneOptionSet clone_options_;
  HeapVector<Member<PartRoot>> cloned_part_root_stack_;
  VectorOf<AtomicString> attribute_values_{};
  wtf_size_t current_attribute_index_{0};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_CLONING_DATA_H_
