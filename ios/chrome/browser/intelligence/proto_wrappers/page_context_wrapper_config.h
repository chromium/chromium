// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_CONFIG_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_CONFIG_H_

#import <Foundation/Foundation.h>

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

 private:
  friend class PageContextWrapperConfigBuilder;

  // Private constructor forces usage of the Builder.
  explicit PageContextWrapperConfig(bool use_refactored_extractor,
                                    bool graft_cross_origin_frame_content,
                                    bool use_rich_extraction);

  // Bit to use the refactored PageContextExtractor.
  bool use_refactored_extractor_;

  // Bit to graft cross-origin frames.
  bool graft_cross_origin_frame_content_;

  // Bit to use the TreeWalker (Rich Extraction).
  bool use_rich_extraction_;
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

  // Returns the PageContextWrapperConfig.
  PageContextWrapperConfig Build() const;

 private:
  bool use_refactored_extractor_;
  bool graft_cross_origin_frame_content_;
  bool use_rich_extraction_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_CONFIG_H_
