// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_TARGET_HISTOGRAM_H_
#define MEDIA_LEARNING_IMPL_TARGET_HISTOGRAM_H_

#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// Histogram of target values that allows fractional counts.
class COMPONENT_EXPORT(LEARNING_IMPL) TargetHistogram {
 private:
  // We use a flat_map since this will often have only one or two TargetValues,
  // such as "true" or "false".
  using CountMap = base::flat_map<TargetValue, double>;

 public:
  TargetHistogram();
  TargetHistogram(const TargetHistogram& rhs);
  TargetHistogram(TargetHistogram&& rhs);
  ~TargetHistogram();

  TargetHistogram& operator=(const TargetHistogram& rhs);
  TargetHistogram& operator=(TargetHistogram&& rhs);

  bool operator==(const TargetHistogram& rhs) const;

  // Add |rhs| to our counts.
  TargetHistogram& operator+=(const TargetHistogram& rhs);

  // Increment |rhs| by one.
  TargetHistogram& operator+=(const TargetValue& rhs);

  // Increment the histogram by |example|'s target value and weight.
  TargetHistogram& operator+=(const LabelledExample& example);

  // Return the number of counts for |value|.
  double operator[](const TargetValue& value) const;
  double& operator[](const TargetValue& value);

  // Return the total counts in the map.
  double total_counts() const {
    double total = 0.;
    for (auto& entry : counts_)
      total += entry.second;
    return total;
  }

  CountMap::const_iterator begin() const { return counts_.begin(); }

  CountMap::const_iterator end() const { return counts_.end(); }

  // Return the number of buckets in the histogram.
  // TODO(liberato): Do we want this?
  size_t size() const { return counts_.size(); }

  // Find the singular value with the highest counts, and copy it into
  // |value_out| and (optionally) |counts_out|.  Returns true if there is a
  // singular maximum, else returns false with the out params undefined.
  bool FindSingularMax(TargetValue* value_out,
                       double* counts_out = nullptr) const;

  // Return the average value of the entries in this histogram.  Of course,
  // this only makes sense if the TargetValues can be interpreted as numeric.
  double Average() const;

  // Normalize the histogram so that it has one total count, unless it's
  // empty.  It will continue to have zero in that case.
  void Normalize();

  std::string ToString() const;

 private:
  const CountMap& counts() const { return counts_; }

  // [value] == counts
  CountMap counts_;

  // Allow copy and assign.
};

COMPONENT_EXPORT(LEARNING_IMPL)
std::ostream& operator<<(std::ostream& out, const TargetHistogram& dist);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_TARGET_HISTOGRAM_H_
