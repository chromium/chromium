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

 private:
  friend class PageContextWrapperConfigBuilder;

  // Private constructor forces usage of the Builder.
  explicit PageContextWrapperConfig(bool use_refactored_extractor,
                                    bool graft_cross_origin_frame_content);

  // Bit to use the refactored PageContextExtractor.
  bool use_refactored_extractor_;

  // Bit to graft cross-origin frames.
  bool graft_cross_origin_frame_content_;
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

  // Returns the PageContextWrapperConfig.
  PageContextWrapperConfig Build() const;

 private:
  bool use_refactored_extractor_;
  bool graft_cross_origin_frame_content_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_CONFIG_H_
