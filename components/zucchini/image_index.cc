// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/image_index.h"

#include <algorithm>
#include <utility>

#include "components/zucchini/algorithm.h"
#include "components/zucchini/disassembler.h"

namespace zucchini {

ImageIndex::ImageIndex(ConstBufferView image)
    : image_(image), type_tags_(image.size(), kNoTypeTag) {}

ImageIndex::ImageIndex(ImageIndex&&) = default;

ImageIndex::~ImageIndex() = default;

bool ImageIndex::Initialize(Disassembler* disasm) {
  std::vector<ReferenceGroup> ref_groups = disasm->MakeReferenceGroups();
  for (const auto& group : ref_groups) {
    // Build pool-to-type mapping.
    DCHECK_NE(kNoPoolTag, group.pool_tag());
    TargetPool& target_pool = target_pools_[group.pool_tag()];
    target_pool.AddType(group.type_tag());
    target_pool.InsertTargets(std::move(*group.GetReader(disasm)));
  }
  for (const auto& group : ref_groups) {
    // Find and store all references for each type, returns false on finding
    // any overlap, to signal error.
    if (!InsertReferences(group.traits(),
                          std::move(*group.GetReader(disasm)))) {
      return false;
    }
  }
  return true;
}

bool ImageIndex::IsToken(offset_t location) const {
  TypeTag type = LookupType(location);

  // |location| points into raw data.
  if (type == kNoTypeTag)
    return true;

  // |location| points into a Reference.
  Reference reference = refs(type).at(location);
  // Only the first byte of a reference is a token.
  return location == reference.location;
}

bool ImageIndex::InsertReferences(const ReferenceTypeTraits& traits,
                                  ReferenceReader&& ref_reader) {
  // Store ReferenceSet for current type (of |group|).
  DCHECK_NE(kNoTypeTag, traits.type_tag);
  auto result = reference_sets_.emplace(
      traits.type_tag, ReferenceSet(traits, pool(traits.pool_tag)));
  DCHECK(result.second);

  result.first->second.InitReferences(std::move(ref_reader));
  for (auto ref : reference_sets_.at(traits.type_tag)) {
    DCHECK(RangeIsBounded(ref.location, traits.width, size()));
    auto cur_type_tag = type_tags_.begin() + ref.location;

    // Check for overlap with existing reference. If found, then invalidate.
    if (std::any_of(cur_type_tag, cur_type_tag + traits.width,
                    [](TypeTag type) { return type != kNoTypeTag; })) {
      return false;
    }
    std::fill(cur_type_tag, cur_type_tag + traits.width, traits.type_tag);
  }
  return true;
}

}  // namespace zucchini
