// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/label_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "courgette/consecutive_range_visitor.h"

namespace courgette {

LabelManager::SimpleIndexAssigner::SimpleIndexAssigner(LabelVector* labels)
    : labels_(labels) {
  // Initialize |num_index_| and |available_|.
  num_index_ = std::max(base::checked_cast<int>(labels_->size()),
                        GetLabelIndexBound(*labels_));
  available_.resize(num_index_, true);
  size_t used = 0;
  for (const Label& label : *labels_) {
    if (label.index_ != Label::kNoIndex) {
      available_.at(label.index_) = false;
      ++used;
    }
  }
  VLOG(1) << used << " of " << labels_->size() << " labels pre-assigned.";
}

LabelManager::SimpleIndexAssigner::~SimpleIndexAssigner() = default;

void LabelManager::SimpleIndexAssigner::DoForwardFill() {
  size_t count = 0;
  // Inside the loop, if |prev_index| == |kNoIndex| then we try to assign 0.
  // This allows 0 (if unused) to be assigned in middle of |labels_|.
  int prev_index = Label::kNoIndex;
  for (auto p = labels_->begin(); p != labels_->end(); ++p) {
    if (p->index_ == Label::kNoIndex) {
      int index = (prev_index == Label::kNoIndex) ? 0 : prev_index + 1;
      if (index < num_index_ && available_.at(index)) {
        p->index_ = index;
        available_.at(index) = false;
        ++count;
      }
    }
    prev_index = p->index_;
  }
  VLOG(1) << "  fill forward " << count;
}

void LabelManager::SimpleIndexAssigner::DoBackwardFill() {
  size_t count = 0;
  // This is asymmetric from DoForwardFill(), to preserve old behavior.
  // Inside the loop, if |prev_index| == |kNoIndex| then we skip assignment.
  // But we initilaize |prev_index| = |num_index_|, so if the last element in
  // |labels_| has no index, then can use |num_index_| - 1 (if unused). We don't
  // try this assignment elsewhere.
  int prev_index = num_index_;
  for (auto p = labels_->rbegin(); p != labels_->rend(); ++p) {
    if (p->index_ == Label::kNoIndex && prev_index != Label::kNoIndex) {
      int index = prev_index - 1;
      if (index >= 0 && available_.at(index)) {
        p->index_ = index;
        available_.at(index) = false;
        ++count;
      }
    }
    prev_index = p->index_;
  }
  VLOG(1) << "  fill backward " << count;
}

void LabelManager::SimpleIndexAssigner::DoInFill() {
  size_t count = 0;
  int index = 0;
  for (Label& label : *labels_) {
    if (label.index_ == Label::kNoIndex) {
      while (!available_.at(index))
        ++index;
      label.index_ = index;
      available_.at(index) = false;
      ++index;
      ++count;
    }
  }
  VLOG(1) << "  infill " << count;
}

LabelManager::LabelManager() = default;

LabelManager::~LabelManager() = default;

// static
int LabelManager::GetLabelIndexBound(const LabelVector& labels) {
  int max_index = -1;
  for (const Label& label : labels) {
    if (label.index_ != Label::kNoIndex)
      max_index = std::max(max_index, label.index_);
  }
  return max_index + 1;
}

// Uses binary search to find |rva|.
Label* LabelManager::Find(RVA rva) {
  auto it = std::lower_bound(
      labels_.begin(), labels_.end(), Label(rva),
      [](const Label& l1, const Label& l2) { return l1.rva_ < l2.rva_; });
  return it == labels_.end() || it->rva_ != rva ? nullptr : &(*it);
}

void LabelManager::UnassignIndexes() {
  for (Label& label : labels_)
    label.index_ = Label::kNoIndex;
}

void LabelManager::DefaultAssignIndexes() {
  int cur_index = 0;
  for (Label& label : labels_) {
    CHECK_EQ(Label::kNoIndex, label.index_);
    label.index_ = cur_index++;
  }
}

void LabelManager::AssignRemainingIndexes() {
  // This adds some memory overhead, about 1 bit per Label (more if indexes >=
  // |labels_.size()| get used).
  SimpleIndexAssigner assigner(&labels_);
  assigner.DoForwardFill();
  assigner.DoBackwardFill();
  assigner.DoInFill();
}

// We wish to minimize peak memory usage here. Analysis: Let
//   m = number of (RVA) elements in |rva_visitor|,
//   n = number of distinct (RVA) elements in |rva_visitor|.
// The final storage is n * sizeof(Label) bytes. During computation we uniquify
// m RVAs, and count repeats. Taking sizeof(RVA) = 4, an implementation using
// std::map or std::unordered_map would consume additionally 32 * n bytes.
// Meanwhile, our std::vector implementation consumes additionally 4 * m bytes
// For our typical usage (i.e. Chrome) we see m = ~4n, so we use 16 * n bytes of
// extra contiguous memory during computation. Assuming memory fragmentation
// would not be an issue, this is much better than using std::map.
void LabelManager::Read(RvaVisitor* rva_visitor) {
  // Write all values in |rva_visitor| to |rvas|.
  size_t num_rva = rva_visitor->Remaining();
  std::vector<RVA> rvas(num_rva);
  for (size_t i = 0; i < num_rva; ++i, rva_visitor->Next())
    rvas[i] = rva_visitor->Get();

  // Sort |rvas|, then count the number of distinct values.
  using CRV = ConsecutiveRangeVisitor<std::vector<RVA>::iterator>;
  std::sort(rvas.begin(), rvas.end());
  DCHECK(rvas.empty() || rvas.back() != kUnassignedRVA);

  size_t num_distinct_rva = 0;
  for (CRV it(rvas.begin(), rvas.end()); it.has_more(); it.advance())
    ++num_distinct_rva;

  // Reserve space for |labels_|, populate with sorted RVA and repeats.
  DCHECK(labels_.empty());
  labels_.reserve(num_distinct_rva);
  for (CRV it(rvas.begin(), rvas.end()); it.has_more(); it.advance()) {
    labels_.push_back(Label(*it.cur()));
    labels_.back().count_ =
        base::checked_cast<decltype(labels_.back().count_)>(it.repeat());
  }
}

}  // namespace courgette
