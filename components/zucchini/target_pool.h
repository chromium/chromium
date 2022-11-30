// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TARGET_POOL_H_
#define COMPONENTS_ZUCCHINI_TARGET_POOL_H_

#include <stddef.h>

#include <deque>
#include <vector>

#include "components/zucchini/image_utils.h"
#include "components/zucchini/patch_reader.h"

namespace zucchini {

class OffsetMapper;
class TargetSource;

// Ordered container of distinct targets that have the same semantics, along
// with a list of associated reference types, only used during patch generation.
class TargetPool {
 public:
  using const_iterator = std::deque<offset_t>::const_iterator;

  TargetPool();
  // Initializes the object with given sorted and unique |targets|.
  explicit TargetPool(std::deque<offset_t>&& targets);
  TargetPool(TargetPool&&);
  TargetPool(const TargetPool&);
  ~TargetPool();

  // Insert new targets from various sources. These invalidate all previous key
  // lookups.
  // - From a list of targets, useful for adding extra targets in Zucchini-gen:
  void InsertTargets(const std::vector<offset_t>& targets);
  // - From TargetSource, useful for adding extra targets in Zucchini-apply:
  void InsertTargets(TargetSource* targets);
  // - From list of References, useful for listing targets in Zucchini-gen:
  void InsertTargets(const std::vector<Reference>& references);
  // - From ReferenceReader, useful for listing targets in Zucchini-apply:
  void InsertTargets(ReferenceReader&& references);

  // Adds |type| as a reference type associated with the pool of targets.
  void AddType(TypeTag type) { types_.push_back(type); }

  // Returns a canonical key associated with a valid target at |offset|.
  key_t KeyForOffset(offset_t offset) const;

  // Returns a canonical key associated with the target nearest to |offset|.
  key_t KeyForNearestOffset(offset_t offset) const;

  // Returns the target for a |key|, which is assumed to be valid and held by
  // this class.
  offset_t OffsetForKey(key_t key) const { return targets_[key]; }

  // Returns whether a particular key is valid.
  bool KeyIsValid(key_t key) const { return key < targets_.size(); }

  // Uses |offset_mapper| to transform "old" |targets_| to "new" |targets_|,
  // resulting in sorted and unique targets.
  void FilterAndProject(const OffsetMapper& offset_mapper);

  // Accessors for testing.
  const std::deque<offset_t>& targets() const { return targets_; }
  const std::vector<TypeTag>& types() const { return types_; }

  // Returns the number of targets.
  size_t size() const { return targets_.size(); }
  const_iterator begin() const { return targets_.cbegin(); }
  const_iterator end() const { return targets_.cend(); }

 private:
  std::vector<TypeTag> types_;     // Enumerates type_tag for this pool.
  std::deque<offset_t> targets_;   // Targets for pool in ascending order.
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TARGET_POOL_H_
