// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"

// TODO(crbug.com/463981051): Remove the use_refactored_extractor option when
// we clean up the feature switch.

PageContextWrapperConfig::PageContextWrapperConfig(
    const PageContextWrapperConfig&) = default;
PageContextWrapperConfig& PageContextWrapperConfig::operator=(
    const PageContextWrapperConfig&) = default;

PageContextWrapperConfig::PageContextWrapperConfig(
    bool use_refactored_extractor)
    : use_refactored_extractor_(use_refactored_extractor) {}

PageContextWrapperConfig::~PageContextWrapperConfig() = default;

bool PageContextWrapperConfig::use_refactored_extractor() const {
  return use_refactored_extractor_;
}

PageContextWrapperConfigBuilder::PageContextWrapperConfigBuilder() {
  use_refactored_extractor_ =
      base::FeatureList::IsEnabled(kPageContextExtractorRefactored);
}

PageContextWrapperConfigBuilder::~PageContextWrapperConfigBuilder() = default;

PageContextWrapperConfigBuilder&
PageContextWrapperConfigBuilder::SetUseRefactoredExtractor(
    bool use_refactored_extractor) {
  use_refactored_extractor_ = use_refactored_extractor;
  return *this;
}

PageContextWrapperConfig PageContextWrapperConfigBuilder::Build() const {
  return PageContextWrapperConfig(use_refactored_extractor_);
}
