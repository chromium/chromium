// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_LABEL_MANAGER_H_
#define COURGETTE_LABEL_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "courgette/image_utils.h"

namespace courgette {

using LabelVector = std::vector<Label>;
using RVAToLabel = std::map<RVA, Label*>;

// A container to store and manage Label instances, dedicated to reducing peak
// memory usage. To this end we preallocate Label instances in bulk, and
// carefully control transient memory usage when initializing Labels.
class LabelManager {
 public:
  // A helper class to heuristically complete index assignment for a list of
  // Labels that have partially assigned indexes.
  // Goal: An address table compresses best when each index is associated with
  // an address that is slightly larger than the previous index.
  // For example, suppose we have RVAs
  //   [0x0000, 0x0070, 0x00E0, 0x0150].
  // Consider candidate (RVA, index) assignments A and B:
  //   A: [(0x0000, 0), (0x0070, 1), (0x00E0, 2), (0x0150, 3)],
  //   B: [(0x0000, 2), (0x0070, 1), (0x00E0, 0), (0x0150, 3)].
  // To form the address table, we first sort by indexes:
  //   A: [(0x0000, 0), (0x0070, 1), (0x00E0, 2), (0x0150, 3)],
  //   B: [(0x00E0, 0), (0x0070, 1), (0x0000, 2), (0x0150, 3)].
  // Then we extract the RVAs for storage:
  //   A: [0x0000, 0x0070, 0x00E0, 0x0150],
  //   B: [0x00E0, 0x0070, 0x0000, 0x0150].
  // Clearly A compresses better than B under (signed) delta encoding.
  // In Courgette-gen, an assignment algorithm (subclass of AdjustmentMethod)
  // creates partial and arbitrary index assignments (to attempt to match one
  // file against another). So the sorted case (like A) won't happen in general.
  // Our helper class fills in the missing assignments by creating runs of
  // consecutive indexes, so once RVAs are sorted by these indexes we'd reduce
  // distances between successive RVAs.
  class SimpleIndexAssigner {
   public:
    explicit SimpleIndexAssigner(LabelVector* labels);
    ~SimpleIndexAssigner();

    // Scans forward to assign successive indexes to Labels, using existing
    // indexes as start-anchors.
    void DoForwardFill();

    // Scans backward to assign successive indexes to Labels, using existing
    // indexes as end-anchors.
    void DoBackwardFill();

    // Assigns all unsigned indexes using what's available, disregarding current
    // Label assignment.
    void DoInFill();

   private:
    // The target LabelVector, owned by the caller.
    LabelVector* labels_;

    // A bound on indexes.
    int num_index_ = 0;

    // Tracker for index usage to ensure uniqueness of indexes.
    std::vector<bool> available_;

    DISALLOW_COPY_AND_ASSIGN(SimpleIndexAssigner);
  };

  LabelManager();
  ~LabelManager();

  // Returns an exclusive upper bound for all assigned indexes in |labels|.
  static int GetLabelIndexBound(const LabelVector& labels);

  // Accessor to stored Label instances.
  const LabelVector& Labels() const { return labels_; }

  // Efficiently searches for a Label that targets |rva|. Returns the pointer to
  // the stored Label instance if found, or null otherwise. Non-const to support
  // implementations that allocate-on-read.
  Label* Find(RVA rva);

  // Removes Label instances whose |count_| is less than |count_threshold|.
  void RemoveUnderusedLabels(int32_t count_threshold);

  // Resets all indexes to an unassigned state.
  void UnassignIndexes();

  // Assigns indexes to successive integers from 0, ordered by RVA.
  void DefaultAssignIndexes();

  // Assigns indexes to any Label instances that don't have one yet.
  void AssignRemainingIndexes();

  // Populates |labels_| using RVAs from |rva_visitor|. Each distinct RVA from
  // |rva_visitor| yields a Label with |rva_| assigned as the RVA, and |count_|
  // assigned as the repeat.
  void Read(RvaVisitor* rva_visitor);

 protected:
  FRIEND_TEST_ALL_PREFIXES(LabelManagerTest, TrivialAssign);
  FRIEND_TEST_ALL_PREFIXES(LabelManagerTest, AssignRemainingIndexes);

  // The main list of Label instances, sorted by the |rva_| member.
  LabelVector labels_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LabelManager);
};

}  // namespace courgette

#endif  // COURGETTE_LABEL_MANAGER_H_
