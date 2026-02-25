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
    bool use_refactored_extractor,
    bool graft_cross_origin_frame_content,
    bool use_rich_extraction)
    : use_refactored_extractor_(use_refactored_extractor),
      graft_cross_origin_frame_content_(graft_cross_origin_frame_content),
      use_rich_extraction_(use_rich_extraction) {}

PageContextWrapperConfig::~PageContextWrapperConfig() = default;

bool PageContextWrapperConfig::use_refactored_extractor() const {
  return use_refactored_extractor_;
}

bool PageContextWrapperConfig::graft_cross_origin_frame_content() const {
  return graft_cross_origin_frame_content_ || use_rich_extraction_;
}

bool PageContextWrapperConfig::use_rich_extraction() const {
  return use_rich_extraction_;
}

PageContextWrapperConfigBuilder::PageContextWrapperConfigBuilder() {
  use_refactored_extractor_ = IsPageContextExtractorRefactoredEnabled();
  graft_cross_origin_frame_content_ = false;
  use_rich_extraction_ = false;
}

PageContextWrapperConfigBuilder::~PageContextWrapperConfigBuilder() = default;

PageContextWrapperConfigBuilder&
PageContextWrapperConfigBuilder::SetUseRefactoredExtractor(
    bool use_refactored_extractor) {
  use_refactored_extractor_ = use_refactored_extractor;
  return *this;
}

PageContextWrapperConfigBuilder&
PageContextWrapperConfigBuilder::SetGraftCrossOriginFrameContent(
    bool graft_cross_origin_frame_content) {
  graft_cross_origin_frame_content_ = graft_cross_origin_frame_content;
  return *this;
}

PageContextWrapperConfigBuilder&
PageContextWrapperConfigBuilder::SetUseRichExtraction(
    bool use_rich_extraction) {
  use_rich_extraction_ = use_rich_extraction;
  return *this;
}

PageContextWrapperConfig PageContextWrapperConfigBuilder::Build() const {
  return PageContextWrapperConfig(use_refactored_extractor_,
                                  graft_cross_origin_frame_content_,
                                  use_rich_extraction_);
}
