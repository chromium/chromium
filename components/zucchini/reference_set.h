// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_REFERENCE_SET_H_
#define COMPONENTS_ZUCCHINI_REFERENCE_SET_H_

#include <stddef.h>

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

class TargetPool;

// Container of distinct references of one type, along with traits, only used
// during patch generation.
class ReferenceSet {
 public:
  using const_iterator = std::vector<Reference>::const_iterator;

  // |traits| specifies the reference represented. |target_pool| specifies
  // common targets shared by all reference represented, and mediates target
  // translation between offsets and indexes.
  ReferenceSet(const ReferenceTypeTraits& traits,
               const TargetPool& target_pool);
  ReferenceSet(const ReferenceSet&) = delete;
  ReferenceSet(ReferenceSet&&);
  ~ReferenceSet();

  // Either one of the initializers below should be called exactly once. These
  // insert all references from |ref_reader/refs| into this class. The targets
  // of these references must be in |target_pool_|.
  void InitReferences(ReferenceReader&& ref_reader);
  void InitReferences(const std::vector<Reference>& refs);

  const std::vector<Reference>& references() const { return references_; }
  const ReferenceTypeTraits& traits() const { return traits_; }
  const TargetPool& target_pool() const { return *target_pool_; }
  TypeTag type_tag() const { return traits_.type_tag; }
  PoolTag pool_tag() const { return traits_.pool_tag; }
  offset_t width() const { return traits_.width; }

  // Looks up the Reference by an |offset| that it spans. |offset| is assumed to
  // be valid, i.e., |offset| must be spanned by some Reference in
  // |references_|.
  Reference at(offset_t offset) const;

  size_t size() const { return references_.size(); }
  const_iterator begin() const { return references_.begin(); }
  const_iterator end() const { return references_.end(); }

 private:
  ReferenceTypeTraits traits_;
  const raw_ref<const TargetPool> target_pool_;
  // List of distinct Reference instances sorted by location.
  std::vector<Reference> references_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_REFERENCE_SET_H_
