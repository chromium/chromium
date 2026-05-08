// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_CONFIG_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_CONFIG_H_

#import <Foundation/Foundation.h>

#import <string>

// Configuration for the PageContextWrapper.
class PageContextWrapperConfig {
 public:
  PageContextWrapperConfig(const PageContextWrapperConfig&);
  PageContextWrapperConfig& operator=(const PageContextWrapperConfig&);
  ~PageContextWrapperConfig();

  // True to use the refactored PageContextExtractor.
  bool use_refactored_extractor() const;

  // True to graft cross-origin frames. When true, the extractor will return
  // remote frame tokens for cross-origin frames instead of skipping them. These
  // tokens are then used to match with the content of the corresponding frames
  // (which content is extracted separately) and graft them into the APC tree,
  // preserving the tree structure.
  bool graft_cross_origin_frame_content() const;

  // True to use the TreeWalker for Page Context extraction (Rich Extraction).
  bool use_rich_extraction() const;

  // True to extract actionable information alongside rich extraction.
  // This needs and will implicitly activate rich extraction.
  bool use_rich_extraction_with_actionable() const;

  // True to extract paid content from the page context.
  // This needs and will implicitly activate rich extraction.
  bool extract_paid_content() const;

  // True to attempt to fix malformed paid content JSON.
  // This needs and will implicitly activate rich extraction.
  bool attempt_paid_content_json_fixing() const;

  // Returns the variant of the configuration to inject into the histograms.
  // Does not include all config bits, only structure-defining ones
  // ("InnerTextOnly", "Rich", and "RichAndActionable").
  std::string GetApcConfigVariant() const;

  // True to extract autofill metadata.
  bool extract_autofill() const;

  // True to apply redacting metadata for credit card numbers.
  bool extract_autofill_credit_card_redactions() const;

 private:
  friend class PageContextWrapperConfigBuilder;

  // Private constructor forces usage of the Builder.
  explicit PageContextWrapperConfig(
      bool use_refactored_extractor,
      bool graft_cross_origin_frame_content,
      bool use_rich_extraction,
      bool use_rich_extraction_with_actionable,
      bool extract_paid_content,
      bool attempt_paid_content_json_fixing,
      bool extract_autofill,
      bool extract_autofill_credit_card_redactions);

  // Bit to use the refactored PageContextExtractor.
  bool use_refactored_extractor_;

  // Bit to graft cross-origin frames.
  bool graft_cross_origin_frame_content_;

  // Bit to use the TreeWalker (Rich Extraction).
  bool use_rich_extraction_;

  // Bit to use the TreeWalker (Rich Extraction) with actionable Mode.
  bool use_rich_extraction_with_actionable_;

  // Bit to extract paid content.
  bool extract_paid_content_;

  // Bit to attempt to fix malformed paid content JSON.
  bool attempt_paid_content_json_fixing_;

  // Bit to extract autofill metadata.
  bool extract_autofill_;

  // Bit to apply Autofill credit card redaction policies.
  bool extract_autofill_credit_card_redactions_;
};

// Builder for PageContextWrapperConfig.
class PageContextWrapperConfigBuilder {
 public:
  PageContextWrapperConfigBuilder();
  ~PageContextWrapperConfigBuilder();

  // Sets whether to use the refactored content extractor.
  PageContextWrapperConfigBuilder& SetUseRefactoredExtractor(
      bool use_refactored_extractor);

  // Sets whether to graft cross-origin frames.
  PageContextWrapperConfigBuilder& SetGraftCrossOriginFrameContent(
      bool graft_cross_origin_frame_content);

  // Sets whether to use the TreeWalker (Rich Extraction).
  PageContextWrapperConfigBuilder& SetUseRichExtraction(
      bool use_rich_extraction);

  // Sets whether to extract actionable information alongside rich extraction.
  // This needs and will implicitly activate rich extraction.
  PageContextWrapperConfigBuilder& SetUseRichExtractionWithActionable(
      bool use_rich_extraction_with_actionable);

  // Sets whether to extract paid content.
  // This needs and will implicitly activate rich extraction.
  PageContextWrapperConfigBuilder& SetExtractPaidContent(
      bool extract_paid_content);

  // Sets whether to attempt to fix malformed paid content JSON.
  // This needs and will implicitly activate rich extraction.
  PageContextWrapperConfigBuilder& SetAttemptPaidContentJsonFixing(
      bool attempt_paid_content_json_fixing);

  // Sets whether to extract autofill metadata. Does the equivalent of the
  // kAnnotatedPageContentWithAutofillAnnotations kill switch in
  // components/optimization_guide/content/browser/page_content_proto_util.cc
  // for blink.
  PageContextWrapperConfigBuilder& SetExtractAutofill(bool extract_autofill);

  // Sets whether to apply Autofill credit card redaction to field values. Does
  // the equivalent of the kAnnotatedPageContentAutofillCreditCardRedactions
  // feature switch in
  // components/optimization_guide/content/browser/page_content_proto_util.cc
  // for blink.
  PageContextWrapperConfigBuilder& SetExtractAutofillCreditCardRedactions(
      bool extract_autofill_credit_card_redactions);

  // Returns the PageContextWrapperConfig.
  PageContextWrapperConfig Build() const;

 private:
  bool use_refactored_extractor_;
  bool graft_cross_origin_frame_content_;
  bool use_rich_extraction_;
  bool use_rich_extraction_with_actionable_;
  bool extract_paid_content_;
  bool attempt_paid_content_json_fixing_;
  bool extract_autofill_;
  bool extract_autofill_credit_card_redactions_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_CONFIG_H_
