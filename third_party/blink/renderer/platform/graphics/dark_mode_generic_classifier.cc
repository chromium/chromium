// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_generic_classifier.h"

#include "third_party/blink/renderer/platform/graphics/darkmode/darkmode_classifier.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {
namespace {

// Decision tree lower and upper thresholds for grayscale and color images.
const float kLowColorCountThreshold[2] = {0.8125, 0.015137};
const float kHighColorCountThreshold[2] = {1, 0.025635};

DarkModeClassification ClassifyUsingDecisionTree(
    const DarkModeImageClassifier::Features& features) {
  float low_color_count_threshold =
      kLowColorCountThreshold[features.is_colorful];
  float high_color_count_threshold =
      kHighColorCountThreshold[features.is_colorful];

  // Very few colors means it's not a photo, apply the filter.
  if (features.color_buckets_ratio < low_color_count_threshold)
    return DarkModeClassification::kApplyFilter;

  // Too many colors means it's probably photorealistic, do not apply it.
  if (features.color_buckets_ratio > high_color_count_threshold)
    return DarkModeClassification::kDoNotApplyFilter;

  // In-between, decision tree cannot give a precise result.
  return DarkModeClassification::kNotClassified;
}

// The neural network expects these features to be in a specific order within
// the vector. Do not change the order here without also changing the neural
// network code!
Vector<float> ToVector(const DarkModeImageClassifier::Features& features) {
  return {features.is_colorful, features.color_buckets_ratio,
          features.transparency_ratio, features.background_ratio,
          features.is_svg};
}

}  // namespace

DarkModeGenericClassifier::DarkModeGenericClassifier() {}

DarkModeClassification DarkModeGenericClassifier::ClassifyWithFeatures(
    const Features& features) {
  DarkModeClassification result = ClassifyUsingDecisionTree(features);

  // If decision tree cannot decide, we use a neural network to decide whether
  // to filter or not based on all the features.
  if (result == DarkModeClassification::kNotClassified) {
    darkmode_tfnative_model::FixedAllocations nn_temp;
    float nn_out;
    auto feature_vector = ToVector(features);
    darkmode_tfnative_model::Inference(&feature_vector[0], &nn_out, &nn_temp);
    result = nn_out > 0 ? DarkModeClassification::kApplyFilter
                        : DarkModeClassification::kDoNotApplyFilter;
  }

  return result;
}

DarkModeClassification
DarkModeGenericClassifier::ClassifyUsingDecisionTreeForTesting(
    const Features& features) {
  return ClassifyUsingDecisionTree(features);
}

}  // namespace blink
