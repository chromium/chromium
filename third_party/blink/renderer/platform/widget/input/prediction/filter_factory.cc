// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/prediction/filter_factory.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/predictor_factory.h"
#include "ui/base/prediction/empty_filter.h"
#include "ui/base/prediction/one_euro_filter.h"
#include "ui/base/ui_base_features.h"

namespace blink {

namespace {
using input_prediction::FilterType;
using input_prediction::PredictorType;
}  // namespace

FilterFactory::FilterFactory(
    const base::Feature& feature,
    const input_prediction::PredictorType predictor_type,
    const input_prediction::FilterType filter_type)
    : predictor_type_(predictor_type), filter_type_(filter_type) {
  LoadFilterParams(feature, predictor_type, filter_type);
}

FilterFactory::~FilterFactory() {}

void FilterFactory::LoadFilterParams(
    const base::Feature& feature,
    const input_prediction::PredictorType predictor_type,
    const input_prediction::FilterType filter_type) {
  if (filter_type == FilterType::kOneEuro) {
    base::FieldTrialParams one_euro_filter_param = {
        {ui::OneEuroFilter::kParamBeta, ""},
        {ui::OneEuroFilter::kParamMincutoff, ""}};
    double beta, mincutoff;
    // Only save the params if they are given in the fieldtrials params
    if (base::GetFieldTrialParamsByFeature(feature, &one_euro_filter_param) &&
        base::StringToDouble(
            one_euro_filter_param[ui::OneEuroFilter::kParamBeta], &beta) &&
        base::StringToDouble(
            one_euro_filter_param[ui::OneEuroFilter::kParamMincutoff],
            &mincutoff)) {
      FilterParamMapKey param_key = {FilterType::kOneEuro, predictor_type};
      FilterParams param_value = {
          {ui::OneEuroFilter::kParamMincutoff, mincutoff},
          {ui::OneEuroFilter::kParamBeta, beta}};
      filter_params_map_.emplace(param_key, param_value);
    }
  }
}

FilterType FilterFactory::GetFilterTypeFromName(
    const std::string& filter_name) {
  if (filter_name == ::features::kFilterNameOneEuro)
    return FilterType::kOneEuro;
  else
    return FilterType::kEmpty;
}

std::unique_ptr<ui::InputFilter> FilterFactory::CreateFilter() {
  FilterParams filter_params;
  GetFilterParams(filter_type_, predictor_type_, &filter_params);
  if (filter_type_ == FilterType::kOneEuro) {
    if (filter_params.empty()) {
      return std::make_unique<ui::OneEuroFilter>();
    } else {
      return std::make_unique<ui::OneEuroFilter>(
          filter_params.find(ui::OneEuroFilter::kParamMincutoff)->second,
          filter_params.find(ui::OneEuroFilter::kParamBeta)->second);
    }
  } else {
    return std::make_unique<ui::EmptyFilter>();
  }
}

void FilterFactory::GetFilterParams(const FilterType filter_type,
                                    const PredictorType predictor_type,
                                    FilterParams* filter_params) {
  FilterParamMapKey key = {filter_type, predictor_type};
  auto params = filter_params_map_.find(key);
  if (params != filter_params_map_.end()) {
    *filter_params = params->second;
  }
}

}  // namespace blink
