// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/json_feature_provider_source.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

JSONFeatureProviderSource::JSONFeatureProviderSource(const std::string& name)
    : name_(name) {
}

JSONFeatureProviderSource::~JSONFeatureProviderSource() {
}

void JSONFeatureProviderSource::LoadJSON(int resource_id) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          resource_id));
  CHECK(result.has_value())
      << "Could not load features: " << name_ << " " << result.error().message;

  auto* value_as_dict = result->GetIfDict();
  CHECK(value_as_dict) << name_;
  // Ensure there are no key collisions.
  for (const auto item : *value_as_dict) {
    if (dictionary_.Find(item.first))
      LOG(FATAL) << "Key " << item.first << " is defined in " << name_
                 << " JSON feature files more than once.";
  }

  // Merge.
  dictionary_.Merge(std::move(*value_as_dict));
}

}  // namespace extensions
