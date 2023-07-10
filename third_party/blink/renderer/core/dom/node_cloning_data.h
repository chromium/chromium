// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_CLONING_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_CLONING_DATA_H_

#include "base/containers/enum_set.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

enum class CloneOption {
  kIncludeDescendants,
  kIncludeShadowRoots,

  // For `CloneOptionSet`.
  kMinValue = kIncludeDescendants,
  kMaxValue = kIncludeShadowRoots,
};

using CloneOptionSet =
    base::EnumSet<CloneOption, CloneOption::kMinValue, CloneOption::kMaxValue>;

struct CORE_EXPORT NodeCloningData final {
  STACK_ALLOCATED();

 public:
  NodeCloningData() = default;
  NodeCloningData(std::initializer_list<CloneOption> values)
      : clone_options_(values) {}
  NodeCloningData(const NodeCloningData&) = delete;
  NodeCloningData& operator=(const NodeCloningData&) = delete;
  ~NodeCloningData() = default;

  bool Has(CloneOption option) const { return clone_options_.Has(option); }
  void Put(CloneOption option) { clone_options_.Put(option); }

 private:
  CloneOptionSet clone_options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_CLONING_DATA_H_
