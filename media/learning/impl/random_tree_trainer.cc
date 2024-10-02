// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_tree_trainer.h"

#include <math.h>

#include <optional>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace media {
namespace learning {

RandomTreeTrainer::Split::Split() = default;

RandomTreeTrainer::Split::Split(int index) : split_index(index) {}

RandomTreeTrainer::Split::Split(Split&& rhs) = default;

RandomTreeTrainer::Split::~Split() = default;

RandomTreeTrainer::Split& RandomTreeTrainer::Split::operator=(Split&& rhs) =
    default;

RandomTreeTrainer::Split::BranchInfo::BranchInfo() = default;

RandomTreeTrainer::Split::BranchInfo::BranchInfo(BranchInfo&& rhs) = default;

RandomTreeTrainer::Split::BranchInfo::~BranchInfo() = default;

struct InteriorNode : public Model {
  InteriorNode(const LearningTask& task,
               int split_index,
               FeatureValue split_point)
      : split_index_(split_index),
        ordering_(task.feature_descriptions[split_index].ordering),
        split_point_(split_point) {}

  // Model
  TargetHistogram PredictDistribution(const FeatureVector& features) override {
    // Figure out what feature value we should use for the split.
    FeatureValue f;
    switch (ordering_) {
      case LearningTask::Ordering::kUnordered:
        // Use 0 for "!=" and 1 for "==".
        f = FeatureValue(features[split_index_] == split_point_);
        break;
      case LearningTask::Ordering::kNumeric:
        // Use 0 for "<=" and 1 for ">".
        f = FeatureValue(features[split_index_] > split_point_);
        break;
    }

    auto iter = children_.find(f);

    // If we've never seen this feature value, then return nothing.
    if (iter == children_.end())
      return TargetHistogram();

    return iter->second->PredictDistribution(features);
  }

  TargetHistogram PredictDistributionWithMissingValues(
      const FeatureVector& features) {
    TargetHistogram total;
    for (auto& child_pair : children_) {
      TargetHistogram predicted =
          child_pair.second->PredictDistribution(features);
      // TODO(liberato): Normalize?  Weight?
      total += predicted;
    }

    return total;
  }

  // Add |child| has the node for feature value |v|.
  void AddChild(FeatureValue v, std::unique_ptr<Model> child) {
    DCHECK(!children_.contains(v));
    children_.emplace(v, std::move(child));
  }

 private:
  // Feature value that we split on.
  int split_index_ = -1;
  base::flat_map<FeatureValue, std::unique_ptr<Model>> children_;

  // How is our feature value ordered?
  LearningTask::Ordering ordering_;

  // For kNumeric features, this is the split point.
  FeatureValue split_point_;
};

struct LeafNode : public Model {
  LeafNode(const TrainingData& training_data,
           const std::vector<size_t>& training_idx,
           LearningTask::Ordering ordering) {
    for (size_t idx : training_idx)
      distribution_ += training_data[idx];

    // Each leaf gets one vote.
    // See https://en.wikipedia.org/wiki/Bootstrap_aggregating .  TL;DR: the
    // individual trees should average (regression) or vote (classification).
    //
    // TODO(liberato): It's unclear that a leaf should get to vote with an
    // entire distribution; we might want to take the max for kUnordered here.
    // If so, then we might also want to Average() for kNumeric targets, though
    // in that case, the results would be the same anyway.  That's not, of
    // course, guaranteed for all methods of converting |distribution_| into a
    // numeric prediction.  In general, we should provide a single estimate.
    distribution_.Normalize();
  }

  // TreeNode
  TargetHistogram PredictDistribution(const FeatureVector&) override {
    return distribution_;
  }

 private:
  TargetHistogram distribution_;
};

RandomTreeTrainer::RandomTreeTrainer(RandomNumberGenerator* rng)
    : HasRandomNumberGenerator(rng) {}

RandomTreeTrainer::~RandomTreeTrainer() {}

void RandomTreeTrainer::Train(const LearningTask& task,
                              const TrainingData& training_data,
                              TrainedModelCB model_cb) {
  // Start with all the training data.
  std::vector<size_t> training_idx;
  training_idx.reserve(training_data.size());
  for (size_t idx = 0; idx < training_data.size(); idx++)
    training_idx.push_back(idx);

  // It's a little odd that we don't post training.  Perhaps we should.
  auto model = Train(task, training_data, training_idx);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(model_cb), std::move(model)));
}

std::unique_ptr<Model> RandomTreeTrainer::Train(
    const LearningTask& task,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx) {
  if (training_data.empty()) {
    return std::make_unique<LeafNode>(training_data, std::vector<size_t>(),
                                      LearningTask::Ordering::kUnordered);
  }

  DCHECK_EQ(task.feature_descriptions.size(), training_data[0].features.size());

  // Start with all features unused.
  FeatureSet unused_set;
  for (size_t idx = 0; idx < task.feature_descriptions.size(); idx++)
    unused_set.insert(idx);

  return Build(task, training_data, training_idx, unused_set);
}

std::unique_ptr<Model> RandomTreeTrainer::Build(
    const LearningTask& task,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx,
    const FeatureSet& unused_set) {
  DCHECK_GT(training_idx.size(), 0u);

  // TODO: enforce a minimum number of samples.  ExtraTrees uses 2 for
  // classification, and 5 for regression.

  // Remove any constant attributes in |training_data| from |unused_set|.  Also
  // check if our training data has a constant target value.  For both features
  // and the target value, if the Optional has a value then it's the singular
  // value that we've found so far.  If we find a second one, then we'll clear
  // the Optional.
  std::optional<TargetValue> target_value(
      training_data[training_idx[0]].target_value);
  std::vector<std::optional<FeatureValue>> feature_values;
  feature_values.resize(training_data[0].features.size());
  for (size_t feature_idx : unused_set) {
    feature_values[feature_idx] =
        training_data[training_idx[0]].features[feature_idx];
  }
  for (size_t idx : training_idx) {
    const LabelledExample& example = training_data[idx];
    // Record this target value to see if there is more than one.  We skip the
    // insertion if we've already determined that it's not constant.
    if (target_value && target_value != example.target_value)
      target_value.reset();

    // For all features in |unused_set|, see if it's a constant in our subset of
    // the training data.
    for (size_t feature_idx : unused_set) {
      auto& value = feature_values[feature_idx];
      if (value && *value != example.features[feature_idx])
        value.reset();
    }
  }

  // Is the output constant in |training_data|?  If so, then generate a leaf.
  // If we're not normalizing leaves, then this matters since this training data
  // might be split across multiple leaves.
  if (target_value) {
    return std::make_unique<LeafNode>(training_data, training_idx,
                                      task.target_description.ordering);
  }

  // Remove any constant features from the unused set, so that we don't try to
  // split on them.  It would work, but it would be trivially useless.  We also
  // don't want to use one of our potential splits on it.
  FeatureSet new_unused_set = unused_set;
  for (size_t feature_idx : unused_set) {
    auto& value = feature_values[feature_idx];
    if (value)
      new_unused_set.erase(feature_idx);
  }

  // Select the feature subset to consider at this leaf.
  // TODO(liberato): For nominals, with one-hot encoding, we'd give an equal
  // chance to each feature's value.  For example, if F1 has {A, B} and F2 has
  // {C,D,E,F}, then we would pick uniformly over {A,B,C,D,E,F}.  However, now
  // we pick between {F1, F2} then pick between either {A,B} or {C,D,E,F}.  We
  // do this because it's simpler and doesn't seem to hurt anything.
  FeatureSet feature_candidates = new_unused_set;
  // TODO(liberato): Let our caller override this.
  const size_t features_per_split =
      std::max(static_cast<int>(sqrt(feature_candidates.size())), 3);
  // Note that it's okay if there are fewer features left; we'll select all of
  // them instead.
  while (feature_candidates.size() > features_per_split) {
    // Remove a random feature.
    size_t which = rng()->Generate(feature_candidates.size());
    auto iter = feature_candidates.begin();
    for (; which; which--, iter++)
      ;
    feature_candidates.erase(iter);
  }

  // TODO(liberato): Does it help if we refuse to split without an info gain?
  Split best_potential_split;

  // Find the best split among the candidates that we have.
  for (int i : feature_candidates) {
    Split potential_split =
        ConstructSplit(task, training_data, training_idx, i);
    if (potential_split.nats_remaining < best_potential_split.nats_remaining) {
      best_potential_split = std::move(potential_split);
    }
  }

  // Note that we can have a split with no index (i.e., no features left, or no
  // feature was an improvement in nats), or with a single index (had features,
  // but all had the same value).  Either way, we should end up with a leaf.
  if (best_potential_split.branch_infos.size() < 2) {
    // Stop when there is no more tree.
    return std::make_unique<LeafNode>(training_data, training_idx,
                                      task.target_description.ordering);
  }

  // Build an interior node
  std::unique_ptr<InteriorNode> node = std::make_unique<InteriorNode>(
      task, best_potential_split.split_index, best_potential_split.split_point);

  for (auto& branch_iter : best_potential_split.branch_infos) {
    node->AddChild(branch_iter.first,
                   Build(task, training_data, branch_iter.second.training_idx,
                         new_unused_set));
  }

  return node;
}

RandomTreeTrainer::Split RandomTreeTrainer::ConstructSplit(
    const LearningTask& task,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx,
    int split_index) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_idx.size(), 0u);

  Split split(split_index);

  bool is_numeric = task.feature_descriptions[split_index].ordering ==
                    LearningTask::Ordering::kNumeric;

  // TODO(liberato): Consider removing nominal feature support and RF.  That
  // would make this code somewhat simpler.

  // For a numeric split, find the split point.  Otherwise, we'll split on every
  // nominal value that this feature has in |training_data|.
  if (is_numeric) {
    split.split_point =
        FindSplitPoint_Numeric(split.split_index, training_data, training_idx);
  } else {
    split.split_point =
        FindSplitPoint_Nominal(split.split_index, training_data, training_idx);
  }

  // Find the split's feature values and construct the training set for each.
  // I think we want to iterate on the underlying vector, and look up the int in
  // the training data directly.  |total_weight| will hold the total weight of
  // all examples that come into this node.
  double total_weight = 0.;
  for (size_t idx : training_idx) {
    const LabelledExample& example = training_data[idx];
    total_weight += example.weight;

    // Get the value of the |index|-th feature for |example|.
    FeatureValue v_i = example.features[split.split_index];

    // Figure out what value this example would use for splitting.  For nominal,
    // it's 1 or 0, based on whether |v_i| is equal to the split or not.  For
    // numeric, it's whether |v_i| is <= the split point or not (0 for <=, and 1
    // for >).
    FeatureValue split_feature;
    if (is_numeric)
      split_feature = FeatureValue(v_i > split.split_point);
    else
      split_feature = FeatureValue(v_i == split.split_point);

    // Add |v_i| to the right training set.  Remember that emplace will do
    // nothing if the key already exists.
    auto result =
        split.branch_infos.emplace(split_feature, Split::BranchInfo());
    auto iter = result.first;

    Split::BranchInfo& branch_info = iter->second;
    branch_info.training_idx.push_back(idx);
    branch_info.target_histogram += example;
  }

  // Figure out how good / bad this split is.
  switch (task.target_description.ordering) {
    case LearningTask::Ordering::kUnordered:
      ComputeSplitScore_Nominal(&split, total_weight);
      break;
    case LearningTask::Ordering::kNumeric:
      ComputeSplitScore_Numeric(&split, total_weight);
      break;
  }

  return split;
}

void RandomTreeTrainer::ComputeSplitScore_Nominal(
    Split* split,
    double total_incoming_weight) {
  // Compute the nats given that we're at this node.
  split->nats_remaining = 0;
  for (auto& info_iter : split->branch_infos) {
    Split::BranchInfo& branch_info = info_iter.second;

    // |weight_along_branch| is the total weight of examples that would follow
    // this branch in the tree.
    const double weight_along_branch =
        branch_info.target_histogram.total_counts();
    // |p_branch| is the probability of following this branch.
    const double p_branch = weight_along_branch / total_incoming_weight;
    for (auto& iter : branch_info.target_histogram) {
      double p = iter.second / total_incoming_weight;
      // p*log(p) is the expected nats if the answer is |iter|.  We multiply
      // that by the probability of being in this bucket at all.
      split->nats_remaining -= (p * log(p)) * p_branch;
    }
  }
}

void RandomTreeTrainer::ComputeSplitScore_Numeric(
    Split* split,
    double total_incoming_weight) {
  // Compute the nats given that we're at this node.
  split->nats_remaining = 0;
  for (auto& info_iter : split->branch_infos) {
    Split::BranchInfo& branch_info = info_iter.second;

    // |weight_along_branch| is the total weight of examples that would follow
    // this branch in the tree.
    const double weight_along_branch =
        branch_info.target_histogram.total_counts();
    // |p_branch| is the probability of following this branch.
    const double p_branch = weight_along_branch / total_incoming_weight;

    // Compute the average at this node.  Note that we have no idea if the leaf
    // node would actually use an average, but really it should match.  It would
    // be really nice if we could compute the value (or TargetHistogram) as
    // part of computing the split, and have somebody just hand that target
    // distribution to the leaf if it ends up as one.
    double average = branch_info.target_histogram.Average();

    for (auto& iter : branch_info.target_histogram) {
      // Compute the squared error for all |iter.second| counts that each have a
      // value of |iter.first|, when this leaf approximates them as |average|.
      double sq_err = (iter.first.value() - average) *
                      (iter.first.value() - average) * iter.second;
      split->nats_remaining += sq_err * p_branch;
    }
  }
}

FeatureValue RandomTreeTrainer::FindSplitPoint_Numeric(
    size_t split_index,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_idx.size(), 0u);

  // We should either (a) choose the single best split point given all our
  // training data (i.e., choosing between the splits that are equally between
  // adjacent feature values), or (b) choose the best split point by drawing
  // uniformly over the range that contains our feature values.  (a) is
  // appropriate with RandomForest, while (b) is appropriate with ExtraTrees.
  FeatureValue v_min = training_data[training_idx[0]].features[split_index];
  FeatureValue v_max = training_data[training_idx[0]].features[split_index];
  for (size_t idx : training_idx) {
    const LabelledExample& example = training_data[idx];
    // Get the value of the |split_index|-th feature for
    FeatureValue v_i = example.features[split_index];
    if (v_i < v_min)
      v_min = v_i;

    if (v_i > v_max)
      v_max = v_i;
  }

  FeatureValue v_split;
  if (v_max == v_min) {
    // Pick |v_split| to return a trivial split, so that this ends up as a
    // leaf node anyway.
    v_split = v_max;
  } else {
    // Choose a random split point.  Note that we want to end up with two
    // buckets, so we don't have a trivial split.  By picking [v_min, v_max),
    // |v_min| will always be in one bucket and |v_max| will always not be.
    v_split = FeatureValue(
        rng()->GenerateDouble(v_max.value() - v_min.value()) + v_min.value());
  }

  return v_split;
}

FeatureValue RandomTreeTrainer::FindSplitPoint_Nominal(
    size_t split_index,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_idx.size(), 0u);

  // Construct a set of all values for |training_idx|.  We don't care about
  // their relative frequency, since one-hot encoding doesn't.
  // For example, if a feature has 10 "yes" instances and 1 "no" instance, then
  // there's a 50% chance for each to be chosen here.  This is because one-hot
  // encoding would do roughly the same thing: when choosing features, the
  // "is_yes" and "is_no" features that come out of one-hot encoding would be
  // equally likely to be chosen.
  //
  // Important but subtle note: we can't choose a value that's been chosen
  // before for this feature, since that would be like splitting on the same
  // one-hot feature more than once.  Luckily, we won't be asked to do that.  If
  // we choose "Yes" at some level in the tree, then the "==" branch will have
  // trivial features which will be removed from consideration early (we never
  // consider features with only one value), and the != branch won't have any
  // "Yes" values for us to pick at a lower level.
  std::set<FeatureValue> values;
  for (size_t idx : training_idx) {
    const LabelledExample& example = training_data[idx];
    values.insert(example.features[split_index]);
  }

  // Select one uniformly at random.
  size_t which = rng()->Generate(values.size());
  auto it = values.begin();
  for (; which > 0; it++, which--)
    ;
  return *it;
}

}  // namespace learning
}  // namespace media
