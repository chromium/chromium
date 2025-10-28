// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_EXTRACTOR_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_EXTRACTOR_JAVA_SCRIPT_FEATURE_H_

#import <string>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

// Javascript feature extracting context from the page content of a frame.
// This gives back the context in a format that can be ingested by the
// PageContextWrapper to get a optimization_guide::proto::PageContext for the
// page content.
class PageContextExtractorJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static PageContextExtractorJavaScriptFeature* GetInstance();

  // Extracts the page context from the `frame` content. Calls `callback` when
  // done with the page context represented by a base::Value object or `null` if
  // the `timeout` is reached when waiting on the results from the JS call. Set
  // `include_anchors` to true to also include the context for the anchor tags.
  // Supply a unique `nonce` token to prevent double extracting the content from
  // frames during a given round of page context extraction (see
  // PageContextWrapper for more details).
  //
  // Does the context extraction recursively by traversing same-origin nested
  // iframes to retrieve their context as well, constructing a tree
  // structure that follows the iframe hierarchy. Excludes xorigin iframes.
  // iframes are marked as processed with the `nonce` via a DOM attribute to
  // avoid double extraction. Stops extraction (leaving it in a partial state)
  // if the PageContext should be detached, or the frame is not the top-most
  // same-origin frame.
  //
  // The extraction results returned via the `callback` can be in 2 formats
  // depending on the extraction outcome: FrameData if the context could be
  // extracted or DetachData if it was detached, see
  // resources/page_context_extractor.ts for more details.
  void ExtractPageContext(
      web::WebFrame* frame,
      bool include_anchors,
      const std::string& nonce,
      base::TimeDelta timeout,
      base::OnceCallback<void(const base::Value*)> callback);

 private:
  friend class base::NoDestructor<PageContextExtractorJavaScriptFeature>;

  PageContextExtractorJavaScriptFeature();
  ~PageContextExtractorJavaScriptFeature() override;

  PageContextExtractorJavaScriptFeature(
      const PageContextExtractorJavaScriptFeature&) = delete;
  PageContextExtractorJavaScriptFeature& operator=(
      const PageContextExtractorJavaScriptFeature&) = delete;

  // Returns the placeholder replacements for the feature script.
  FeatureScript::PlaceholderReplacements GetReplacements();
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_EXTRACTOR_JAVA_SCRIPT_FEATURE_H_
