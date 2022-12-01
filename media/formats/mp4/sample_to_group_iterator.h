// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_SAMPLE_TO_GROUP_ITERATOR_H_
#define MEDIA_FORMATS_MP4_SAMPLE_TO_GROUP_ITERATOR_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ref.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {
namespace mp4 {

// Sample To Group Box ('sbgp') can be used to find the group that a sample
// belongs to and the associated description of that sample group. It is
// compactly coded though. This class implements the iterator to iterate
// through the compressed table to get the associated sample group description
// index.
class MEDIA_EXPORT SampleToGroupIterator {
 public:
  explicit SampleToGroupIterator(const SampleToGroup& sample_to_group);

  SampleToGroupIterator(const SampleToGroupIterator&) = delete;
  SampleToGroupIterator& operator=(const SampleToGroupIterator&) = delete;

  ~SampleToGroupIterator();

  // Advances the iterator to refer to the next sample. Return status
  // indicating whether the sample is still valid.
  bool Advance();

  // Returns whether the current sample is valid.
  bool IsValid() const;

  // Returns group description index for current sample.
  uint32_t group_description_index() const {
    return iterator_->group_description_index;
  }

 private:
  // Track how many samples remaining for current table entry.
  uint32_t remaining_samples_;
  const raw_ref<const std::vector<SampleToGroupEntry>, DanglingUntriaged>
      sample_to_group_table_;
  std::vector<SampleToGroupEntry>::const_iterator iterator_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_SAMPLE_TO_GROUP_ITERATOR_H_
