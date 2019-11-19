// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "base/test/task_environment.h"
#include "media/learning/impl/learning_task_controller_impl.h"

using media::learning::FeatureValue;
using media::learning::FeatureVector;
using media::learning::LearningTask;
using ValueDescription = media::learning::LearningTask::ValueDescription;
using media::learning::LearningTaskControllerImpl;
using media::learning::ObservationCompletion;
using media::learning::TargetValue;

ValueDescription ConsumeValueDescription(FuzzedDataProvider* provider) {
  ValueDescription desc;
  desc.name = provider->ConsumeRandomLengthString(100);
  desc.ordering = provider->ConsumeEnum<LearningTask::Ordering>();
  desc.privacy_mode = provider->ConsumeEnum<LearningTask::PrivacyMode>();
  return desc;
}

double ConsumeDouble(FuzzedDataProvider* provider) {
  std::vector<uint8_t> v = provider->ConsumeBytes<uint8_t>(sizeof(double));
  if (v.size() == sizeof(double))
    return reinterpret_cast<double*>(v.data())[0];

  return 0;
}

FeatureVector ConsumeFeatureVector(FuzzedDataProvider* provider) {
  FeatureVector features;
  int n = provider->ConsumeIntegralInRange(0, 100);
  while (n-- > 0)
    features.push_back(FeatureValue(ConsumeDouble(provider)));

  return features;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::test::TaskEnvironment task_environment;
  FuzzedDataProvider provider(data, size);

  LearningTask task;
  task.name = provider.ConsumeRandomLengthString(100);
  task.model = provider.ConsumeEnum<LearningTask::Model>();
  task.use_one_hot_conversion = provider.ConsumeBool();
  task.uma_hacky_aggregate_confusion_matrix = provider.ConsumeBool();
  task.uma_hacky_by_training_weight_confusion_matrix = provider.ConsumeBool();
  task.uma_hacky_by_feature_subset_confusion_matrix = provider.ConsumeBool();
  int n_features = provider.ConsumeIntegralInRange(0, 100);
  int subset_size = provider.ConsumeIntegralInRange<uint8_t>(0, n_features);
  if (subset_size)
    task.feature_subset_size = subset_size;
  for (int i = 0; i < n_features; i++)
    task.feature_descriptions.push_back(ConsumeValueDescription(&provider));
  task.target_description = ConsumeValueDescription(&provider);

  LearningTaskControllerImpl controller(task);

  // Build random examples.
  while (provider.remaining_bytes() > 0) {
    base::UnguessableToken id = base::UnguessableToken::Create();
    base::Optional<TargetValue> default_target;
    if (provider.ConsumeBool())
      default_target = TargetValue(ConsumeDouble(&provider));
    controller.BeginObservation(id, ConsumeFeatureVector(&provider),
                                default_target);
    controller.CompleteObservation(
        id, ObservationCompletion(TargetValue(ConsumeDouble(&provider)),
                                  ConsumeDouble(&provider)));
    task_environment.RunUntilIdle();
  }

  return 0;
}
