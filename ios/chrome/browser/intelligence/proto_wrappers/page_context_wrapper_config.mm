// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"

// TODO(crbug.com/463981051): Remove the use_refactored_extractor option when
// we clean up the feature switch.

PageContextWrapperConfig::PageContextWrapperConfig(
    const PageContextWrapperConfig& other)
    : use_refactored_extractor_(other.use_refactored_extractor_),
      graft_cross_origin_frame_content_(
          other.graft_cross_origin_frame_content_),
      use_rich_extraction_(other.use_rich_extraction_),
      use_rich_extraction_with_actionable_(
          other.use_rich_extraction_with_actionable_),
      extract_paid_content_(other.extract_paid_content_),
      attempt_paid_content_json_fixing_(
          other.attempt_paid_content_json_fixing_) {}
PageContextWrapperConfig& PageContextWrapperConfig::operator=(
    const PageContextWrapperConfig&) = default;

PageContextWrapperConfig::PageContextWrapperConfig(
    bool use_refactored_extractor,
    bool graft_cross_origin_frame_content,
    bool use_rich_extraction,
    bool use_rich_extraction_with_actionable,
    bool extract_paid_content,
    bool attempt_paid_content_json_fixing)
    : use_refactored_extractor_(use_refactored_extractor),
      graft_cross_origin_frame_content_(graft_cross_origin_frame_content),
      use_rich_extraction_(use_rich_extraction),
      use_rich_extraction_with_actionable_(use_rich_extraction_with_actionable),
      extract_paid_content_(extract_paid_content),
      attempt_paid_content_json_fixing_(attempt_paid_content_json_fixing) {}

PageContextWrapperConfig::~PageContextWrapperConfig() = default;

bool PageContextWrapperConfig::use_refactored_extractor() const {
  return use_refactored_extractor_;
}

bool PageContextWrapperConfig::graft_cross_origin_frame_content() const {
  return graft_cross_origin_frame_content_ || use_rich_extraction_ ||
         use_rich_extraction_with_actionable_ || extract_paid_content_ ||
         attempt_paid_content_json_fixing_;
}

bool PageContextWrapperConfig::use_rich_extraction() const {
  return use_rich_extraction_;
}

bool PageContextWrapperConfig::use_rich_extraction_with_actionable() const {
  return use_rich_extraction_with_actionable_;
}

bool PageContextWrapperConfig::extract_paid_content() const {
  return extract_paid_content_;
}

bool PageContextWrapperConfig::attempt_paid_content_json_fixing() const {
  return attempt_paid_content_json_fixing_;
}

PageContextWrapperConfigBuilder::PageContextWrapperConfigBuilder() {
  use_refactored_extractor_ = IsPageContextExtractorRefactoredEnabled();
  graft_cross_origin_frame_content_ = false;
  use_rich_extraction_ = false;
  use_rich_extraction_with_actionable_ = false;
  extract_paid_content_ = false;
  attempt_paid_content_json_fixing_ = false;
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

PageContextWrapperConfigBuilder&
PageContextWrapperConfigBuilder::SetUseRichExtractionWithActionable(
    bool use_rich_extraction_with_actionable) {
  use_rich_extraction_with_actionable_ = use_rich_extraction_with_actionable;
  return *this;
}

PageContextWrapperConfigBuilder&
PageContextWrapperConfigBuilder::SetExtractPaidContent(
    bool extract_paid_content) {
  extract_paid_content_ = extract_paid_content;
  return *this;
}

PageContextWrapperConfigBuilder&
PageContextWrapperConfigBuilder::SetAttemptPaidContentJsonFixing(
    bool attempt_paid_content_json_fixing) {
  attempt_paid_content_json_fixing_ = attempt_paid_content_json_fixing;
  return *this;
}

PageContextWrapperConfig PageContextWrapperConfigBuilder::Build() const {
  return PageContextWrapperConfig(
      use_refactored_extractor_, graft_cross_origin_frame_content_,
      use_rich_extraction_, use_rich_extraction_with_actionable_,
      extract_paid_content_, attempt_paid_content_json_fixing_);
}
