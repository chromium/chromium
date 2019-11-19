// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_RANDOM_TREE_TRAINER_H_
#define MEDIA_LEARNING_IMPL_RANDOM_TREE_TRAINER_H_

#include <limits>
#include <map>
#include <memory>
#include <set>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/random_number_generator.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

// Trains RandomTree decision tree classifier / regressor.
//
// Decision trees, including RandomTree, classify instances as follows.  Each
// non-leaf node is marked with a feature number |i|.  The value of the |i|-th
// feature of the instance is then used to select which outgoing edge is
// traversed.  This repeats until arriving at a leaf, which has a distribution
// over target values that is the prediction.  The tree structure, including the
// feature index at each node and distribution at each leaf, is chosen once when
// the tree is trained.
//
// Training involves starting with a set of training examples, each of which has
// features and a target value.  The tree is constructed recursively, starting
// with the root.  For the node being constructed, the training algorithm is
// given the portion of the training set that would reach the node, if it were
// sent down the tree in a similar fashion as described above.  It then
// considers assigning each (unused) feature index as the index to split the
// training examples at this node.  For each index |t|, it groups the training
// set into subsets, each of which consists of those examples with the same
// of the |i|-th feature.  It then computes a score for the split using the
// target values that ended up in each group.  The index with the best score is
// chosen for the split.
//
// For nominal features, we split the feature into all of its nominal values.
// This is somewhat nonstandard; one would normally convert to one-hot numeric
// features first.  See OneHotConverter if you'd like to do this.
//
// For numeric features, we choose a split point uniformly at random between its
// min and max values in the training data.  We do this because it's suitable
// for extra trees.  RandomForest trees want to select the best split point for
// each feature, rather than uniformly.  Either way, of course, we choose the
// best split among the (feature, split point) pairs we're considering.
//
// Also note that for one-hot features, these are the same thing.  So, this
// implementation is suitable for extra trees with numeric (possibly one hot)
// features, or RF with one-hot nominal features.  Note that non-one-hot nominal
// features probably work fine with RF too.  Numeric, non-binary features don't
// work with RF, unless one changes the split point selection.
//
// The training algorithm then recurses to build child nodes.  One child node is
// created for each observed value of the |i|-th feature in the training set.
// The child node is trained using the subset of the training set that shares
// that node's value for feature |i|.
//
// The above is a generic decision tree training algorithm.  A RandomTree
// differs from that mostly in how it selects the feature to split at each node
// during training.  Rather than computing a score for each feature, a
// RandomTree chooses a random subset of the features and only compares those.
//
// See https://en.wikipedia.org/wiki/Random_forest for information.  Note that
// this is just a single tree, not the whole forest.
//
// Note that this variant chooses split points randomly, as described by the
// ExtraTrees algorithm.  This is slightly different than RandomForest, which
// chooses split points to improve the split's score.
class COMPONENT_EXPORT(LEARNING_IMPL) RandomTreeTrainer
    : public TrainingAlgorithm,
      public HasRandomNumberGenerator {
 public:
  explicit RandomTreeTrainer(RandomNumberGenerator* rng = nullptr);
  ~RandomTreeTrainer() override;

  // Train on all examples.  Calls |model_cb| with the trained model, which
  // won't happen before this returns.
  void Train(const LearningTask& task,
             const TrainingData& examples,
             TrainedModelCB model_cb) override;

 private:
  // Train on the subset |training_idx|.
  std::unique_ptr<Model> Train(const LearningTask& task,
                               const TrainingData& examples,
                               const std::vector<size_t>& training_idx);

  // Set of feature indices.
  using FeatureSet = std::set<int>;

  // Information about a proposed split, and the training sets that would result
  // from that split.
  struct Split {
    Split();
    explicit Split(int index);
    Split(Split&& rhs);
    ~Split();

    Split& operator=(Split&& rhs);

    // Feature index to split on.
    size_t split_index = 0;

    // For numeric splits, branch 0 is <= |split_point|, and 1 is > .
    FeatureValue split_point;

    // Expected nats needed to compute the class, given that we're at this
    // node in the tree.
    // "nat" == entropy measured with natural log rather than base-2.
    double nats_remaining = std::numeric_limits<double>::infinity();

    // Per-branch (i.e. per-child node) information about this split.
    struct BranchInfo {
      explicit BranchInfo();
      BranchInfo(const BranchInfo& rhs) = delete;
      BranchInfo(BranchInfo&& rhs);
      ~BranchInfo();

      BranchInfo& operator=(const BranchInfo& rhs) = delete;
      BranchInfo& operator=(BranchInfo&& rhs) = delete;

      // Training set for this branch of the split.  |training_idx| holds the
      // indices that we're using out of our training data.
      std::vector<size_t> training_idx;

      // Number of occurances of each target value in |training_data| along this
      // branch of the split.
      // This is a flat_map since we're likely to have a very small (e.g.,
      // "true / "false") number of targets.
      TargetHistogram target_histogram;
    };

    // [feature value at this split] = info about which examples take this
    // branch of the split.
    std::map<FeatureValue, BranchInfo> branch_infos;

    DISALLOW_COPY_AND_ASSIGN(Split);
  };

  // Build this node from |training_data|.  |used_set| is the set of features
  // that we already used higher in the tree.
  std::unique_ptr<Model> Build(const LearningTask& task,
                               const TrainingData& training_data,
                               const std::vector<size_t>& training_idx,
                               const FeatureSet& used_set);

  // Compute and return a split of |training_data| on the |index|-th feature.
  Split ConstructSplit(const LearningTask& task,
                       const TrainingData& training_data,
                       const std::vector<size_t>& training_idx,
                       int index);

  // Fill in |nats_remaining| for |split| for a nominal target.
  // |total_incoming_weight| is the total weight of all instances coming into
  // the node that we're splitting.
  void ComputeSplitScore_Nominal(Split* split, double total_incoming_weight);

  // Fill in |nats_remaining| for |split| for a numeric target.
  void ComputeSplitScore_Numeric(Split* split, double total_incoming_weight);

  // Compute the split point for |training_data| for a nominal feature.
  FeatureValue FindSplitPoint_Nominal(size_t index,
                                      const TrainingData& training_data,
                                      const std::vector<size_t>& training_idx);

  // Compute the split point for |training_data| for a numeric feature.
  FeatureValue FindSplitPoint_Numeric(size_t index,
                                      const TrainingData& training_data,
                                      const std::vector<size_t>& training_idx);

  DISALLOW_COPY_AND_ASSIGN(RandomTreeTrainer);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_RANDOM_TREE_TRAINER_H_
