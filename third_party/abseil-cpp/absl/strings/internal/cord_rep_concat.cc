// Copyright 2021 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>

#include "absl/base/config.h"
#include "absl/container/inlined_vector.h"
#include "absl/strings/internal/cord_internal.h"
#include "absl/strings/internal/cord_rep_flat.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace cord_internal {

CordRepConcat::ExtractResult CordRepConcat::ExtractAppendBuffer(
    CordRepConcat* tree, size_t extra_capacity) {
  absl::InlinedVector<CordRepConcat*, kInlinedVectorSize> stack;
  CordRepConcat* concat = tree;
  CordRep* rep = concat->right;

  // Dive down the tree, making sure no edges are shared
  while (concat->refcount.IsOne() && rep->IsConcat()) {
    stack.push_back(concat);
    concat = rep->concat();
    rep = concat->right;
  }
  // Validate we ended on a non shared flat.
  if (concat->refcount.IsOne() && rep->IsFlat() &&
      rep->refcount.IsOne()) {
    // Verify it has at least the requested extra capacity
    CordRepFlat* flat = rep->flat();
    size_t remaining = flat->Capacity() - flat->length;
    if (extra_capacity > remaining) return {tree, nullptr};

    // Check if we have a parent to adjust, or if we must return the left node.
    rep = concat->left;
    if (!stack.empty()) {
      stack.back()->right = rep;
      for (CordRepConcat* parent : stack) {
        parent->length -= flat->length;
      }
      rep = tree;
    }
    delete concat;
    return {rep, flat};
  }
  return {tree, nullptr};
}

}  // namespace cord_internal
ABSL_NAMESPACE_END
}  // namespace absl
