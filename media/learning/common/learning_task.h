// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_LEARNING_TASK_H_
#define MEDIA_LEARNING_COMMON_LEARNING_TASK_H_

#include <initializer_list>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// Description of a learning task.  This includes both the description of the
// inputs (features) and output (target value), plus a choice of the model and
// parameters for learning.
// TODO(liberato): Consider separating the task from the choice of model.
// TODO(liberato): should this be in impl?  Probably not if we want to allow
// registering tasks.
struct COMPONENT_EXPORT(LEARNING_COMMON) LearningTask {
  // Numeric ID for this task for UKM reporting.
  using Id = uint64_t;

  // Not all models support all feature / target descriptions.  For example,
  // NaiveBayes requires kUnordered features.  Similarly, LogLinear woudln't
  // support kUnordered features or targets.  kRandomForest might support more
  // combination of orderings and types.
  enum class Model {
    kExtraTrees,
    kLookupTable,

    // For the fuzzer.
    kMaxValue = kLookupTable
  };

  enum class Ordering {
    // Values are not ordered; nearby values might have wildly different
    // meanings.  For example, two ints that are computed by taking the hash
    // of a string are unordered; it's categorical data.  Values of type DOUBLE
    // should almost certainly not be kUnordered; discretize them in some way
    // if you really want to make discrete, unordered buckets out of them.
    kUnordered,

    // Values may be interpreted as being in numeric order.  For example, two
    // ints that represent the number of elapsed milliseconds are numerically
    // ordered in a meaningful way.
    kNumeric,

    // For the fuzzer.
    kMaxValue = kNumeric
  };

  enum class PrivacyMode {
    // Value represents private information, such as a URL that was visited by
    // the user.
    kPrivate,

    // Value does not represent private information, such as video width.
    kPublic,

    // For the fuzzer.
    kMaxValue = kPublic
  };

  // Description of how a Value should be interpreted.
  struct ValueDescription {
    // Name of this value, such as "source_url" or "width".
    std::string name;

    // Is this value nominal or not?
    Ordering ordering = Ordering::kUnordered;

    // Should this value be treated as being private?
    PrivacyMode privacy_mode = PrivacyMode::kPublic;
  };

  LearningTask();
  LearningTask(const std::string& name,
               Model model,
               std::initializer_list<ValueDescription> feature_init_list,
               ValueDescription target_description);
  LearningTask(const LearningTask&);
  ~LearningTask();

  // Return a stable, unique numeric ID for this task.  This requires a stable,
  // unique |name| for the task.  This is used to identify this task in UKM.
  Id GetId() const;

  // Returns a reference to an empty learning task.
  static const LearningTask& Empty();

  // Unique name for this task.
  std::string name;

  Model model = Model::kExtraTrees;

  std::vector<ValueDescription> feature_descriptions;

  // Note that kUnordered targets indicate classification, while kOrdered
  // targes indicate regression.
  ValueDescription target_description;

  // TODO(liberato): add training parameters, like smoothing constants.  It's
  // okay if some of these are model-specific.
  // TODO(liberato): switch to base::DictionaryValue?

  // Maximum data set size until we start replacing examples.
  size_t max_data_set_size = 100u;

  // Fraction of examples that must be new before the task controller will train
  // a new model.  Note that this is a fraction of the number of examples that
  // we currently have, which might be less than |max_data_set_size|.
  double min_new_data_fraction = 0.1;

  // If provided, then we'll randomly select a |*feature_subset_size|-sized set
  // of feature to train the model with, to allow for feature importance
  // measurement.  Note that UMA reporting only supports subsets of size one, or
  // the whole set.
  base::Optional<int> feature_subset_size;

  // RandomForest parameters

  // Number of trees in the random forest.
  size_t rf_number_of_trees = 100;

  // Should ExtraTrees apply one-hot conversion automatically?  RandomTree has
  // been modified to support nominals directly, though it isn't exactly the
  // same as one-hot conversion.  It is, however, much faster.
  bool use_one_hot_conversion = false;

  // Reporting parameters

  // This is a hack for the initial media capabilities investigation. It
  // represents the threshold that we'll use to decide if a prediction would be
  // T / F.  We should not do this -- instead we should report the distribution
  // average for the prediction and the observation via UKM.
  //
  // In particular, if the percentage of dropped frames is greater than this,
  // then report "false" (not smooth), else we report true.
  //
  // A better, non-hacky approach would be to report the predictions and
  // observations directly, and do offline analysis with whatever threshold we
  // like.  This would remove the thresholding requirement, and also permit
  // additional types of analysis for general regression tasks, such measuring
  // the prediction error directly.
  //
  // The UKM reporter will support this.
  double smoothness_threshold = 0.1;

  // If set, then we'll record a confusion matrix (hackily, see
  // |smoothness_threshold|, above, for what that means) to UMA for all
  // predictions.  Add this task's name to histograms.xml, in the histogram
  // suffixes for "Media.Learning.BinaryThreshold.Aggregate".  The threshold is
  // chosen by |smoothness_threshold|.
  //
  // This option is ignored if feature subset selection is in use.
  bool uma_hacky_aggregate_confusion_matrix = false;

  // If set, then we'll record a histogram of many confusion matrices, split out
  // by the total training data weight that was used to construct the model.  Be
  // sure to add this task's name to histograms.xml, in the histogram suffixes
  // for "Media.Learning.BinaryThreshold.ByTrainingWeight".  The threshold is
  // chosen by |smoothness_threshold|.
  //
  // This option is ignored if feature subset selection is in use.
  bool uma_hacky_by_training_weight_confusion_matrix = false;

  // If set, then we'll record a histogram of many confusion matrices, split out
  // by the (single) selected feature subset.  This does nothing if we're not
  // using feature subsets, or if the subset size isn't one.  Be sure to add
  // this tasks' name to histograms.xml, in the histogram suffixes for
  // "Media.Learning.BinaryThreshold.ByFeature" too.
  bool uma_hacky_by_feature_subset_confusion_matrix = false;

  // Maximum training weight for UMA reporting.  We'll report results offset
  // into different confusion matrices in the same histogram, evenly spaced
  // from 0 to |max_reporting_weight|, with one additional bucket for everything
  // larger than that.  The number of buckets is |num_reporting_weight_buckets|.
  // The default value of 0 is special; it means that we should split up the
  // buckets such that the last bucket means "entirely full training set", while
  // the remainder are evenly spaced.  This is the same as setting it to
  // |max_data_set_size - 1|.  Of course, |max_data_set_size| is a number of
  // examples, not a weight, so this only makes any sense at all if all of the
  // examples have the default weight of 1.
  double max_reporting_weight = 0.;

  // Number of buckets that we'll use to split out the confusion matrix by
  // training weight.  The last one is reserved for "all", while the others are
  // split evenly from 0 to |max_reporting_weight|, inclusive.  One can select
  // up to 15 buckets.  We use 11 by default, so it breaks up the default weight
  // into buckets of size 10.
  //
  // In other words, the defaults will make these buckets:
  // [0-9] [10-19] ... [90-99] [100 and up].  This makes sense if the training
  // set maximum size is the default of 100, and each example has a weight of 1.
  int num_reporting_weight_buckets = 11;

  // If set, then we'll record results to UKM.  Note that this may require an
  // additional privacy review for your learning task!  Also note that it is
  // currently exclusive with |uma_hacky_confusion_matrix| for no technical
  // reason whatsoever.
  bool report_via_ukm = false;

  // When reporting via UKM, we will scale observed / predicted values.  These
  // are the minimum and maximum target / observed values that will be
  // representable.  The UKM record will scale / translate this range into
  // 0-100 integer, inclusive.  This is intended for regression targets.
  // Classification will do something else.
  double ukm_min_input_value = 0.0;
  double ukm_max_input_value = 1.0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_LEARNING_TASK_H_
