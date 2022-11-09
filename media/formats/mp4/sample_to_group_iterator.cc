// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/sample_to_group_iterator.h"

#include "base/check.h"

namespace media {
namespace mp4 {

SampleToGroupIterator::SampleToGroupIterator(
    const SampleToGroup& sample_to_group)
    : remaining_samples_(0),
      sample_to_group_table_(sample_to_group.entries),
      iterator_(sample_to_group_table_->begin()) {
  // Handle the case that the table contains an entry with sample count 0.
  while (iterator_ != sample_to_group_table_->end()) {
    remaining_samples_ = iterator_->sample_count;
    if (remaining_samples_ > 0)
      break;
    ++iterator_;
  }
}

SampleToGroupIterator::~SampleToGroupIterator() = default;

bool SampleToGroupIterator::Advance() {
  DCHECK(IsValid());

  --remaining_samples_;
  // Handle the case that the table contains an entry with sample count 0.
  while (remaining_samples_ == 0) {
    ++iterator_;
    if (iterator_ == sample_to_group_table_->end())
      return false;
    remaining_samples_ = iterator_->sample_count;
  }
  return true;
}

bool SampleToGroupIterator::IsValid() const {
  return remaining_samples_ > 0;
}

}  // namespace mp4
}  // namespace media
