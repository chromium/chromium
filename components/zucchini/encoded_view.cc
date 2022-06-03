// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/encoded_view.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"

namespace zucchini {

EncodedView::EncodedView(const ImageIndex& image_index)
    : image_index_(image_index), pool_infos_(image_index.PoolCount()) {}
EncodedView::~EncodedView() = default;

EncodedView::value_type EncodedView::Projection(offset_t location) const {
  DCHECK_LT(location, image_index_.size());

  // Find out what lies at |location|.
  TypeTag type = image_index_.LookupType(location);

  // |location| points into raw data.
  if (type == kNoTypeTag) {
    // The projection is the identity function on raw content.
    return image_index_.GetRawValue(location);
  }

  // |location| points into a Reference.
  const ReferenceSet& ref_set = image_index_.refs(type);
  Reference ref = ref_set.at(location);
  DCHECK_GE(location, ref.location);
  DCHECK_LT(location, ref.location + ref_set.width());

  // |location| is not the first byte of the reference.
  if (location != ref.location) {
    // Trailing bytes of a reference are all projected to the same value.
    return kReferencePaddingProjection;
  }

  PoolTag pool_tag = ref_set.pool_tag();
  const auto& target_pool = ref_set.target_pool();

  // Targets with an associated Label will use its Label index in projection.
  DCHECK_EQ(target_pool.size(), pool_infos_[pool_tag.value()].labels.size());
  uint32_t label = pool_infos_[pool_tag.value()]
                       .labels[target_pool.KeyForOffset(ref.target)];

  // Projection is done on (|target|, |type|), shifted by
  // kBaseReferenceProjection to avoid collisions with raw content.
  value_type projection = label;
  projection *= image_index_.TypeCount();
  projection += type.value();
  return projection + kBaseReferenceProjection;
}

size_t EncodedView::Cardinality() const {
  size_t max_width = 0;
  for (const auto& pool_info : pool_infos_)
    max_width = std::max(max_width, pool_info.bound);
  return max_width * image_index_.TypeCount() + kBaseReferenceProjection;
}

void EncodedView::SetLabels(PoolTag pool,
                            std::vector<uint32_t>&& labels,
                            size_t bound) {
  DCHECK_EQ(labels.size(), image_index_.pool(pool).size());
  DCHECK(labels.empty() || *max_element(labels.begin(), labels.end()) < bound);
  pool_infos_[pool.value()].labels = std::move(labels);
  pool_infos_[pool.value()].bound = bound;
}

EncodedView::PoolInfo::PoolInfo() = default;
EncodedView::PoolInfo::PoolInfo(PoolInfo&&) = default;
EncodedView::PoolInfo::~PoolInfo() = default;

}  // namespace zucchini
