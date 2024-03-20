// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MIXED_CONTENT_CHECKER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MIXED_CONTENT_CHECKER_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class RenderFrameHostImpl;

// Utilities for browser-process-side mixed content security checks.
//
// Changes to this class might need to be reflected on its renderer counterpart,
// i.e. `blink::MixedContentChecker`.
//
// Current mixed content W3C draft that drives this implementation:
// https://w3c.github.io/webappsec-mixed-content/
class CONTENT_EXPORT MixedContentChecker {
 public:
  MixedContentChecker();
  ~MixedContentChecker();
  // Not copyable.
  MixedContentChecker(const MixedContentChecker&) = delete;
  MixedContentChecker& operator=(const MixedContentChecker&) = delete;

  // Checks if a navigation indicated by `navigation_handle` should be blocked
  // or not due to mixed content, and also updates the renderer about Blink
  // mixed content related features from this navigation.
  //
  // Based off of `blink::MixedContentChecker::ShouldBlockFetch()`.
  bool ShouldBlockNavigation(NavigationHandle& navigation_handle,
                             bool for_redirect);

  // Checks if a fetch keepalive request that loads `url` should be blocked or
  // not due to mixed content, without reporting back to renderer.
  //
  // `initiator_frame` is the RenderFrameHostImpl of the document that initiates
  // loading a fetch(`url`, {keepalive: true}) request. Note that as a
  // RenderFrameHostImpl can be used to load a different document (see also
  // docs/render_document.md), it is caller's responsibility to ensure the
  // passed in `initiator_frame` still represents the original document that
  // loads `url` throughout the entire call.
  //
  // Based off of `blink::MixedContentChecker::ShouldBlockFetch()`.
  static bool ShouldBlockFetchKeepAlive(RenderFrameHostImpl* initiator_frame,
                                        const GURL& url,
                                        bool for_redirect);

 private:
  // Common logic to calculate whether `url` is considered mixed content given
  // `mixed_content_frame`, where `node` is the FrameTreeNode of the frame that
  // initiates loading `url`.
  //
  // Based off of `blink::MixedContentChecker::ShouldBlockFetch()`.
  //
  // Sets `cancel_prerendering` to cancel prerendering page.
  //
  // Provides `mixed_content_features` to obtain the Blink features from this
  // method when finding mixed content.
  //
  // `should_report_to_renderer`, if provided, will be set to true when this
  // method tells the caller should report back to renderer when finding mixed
  // content.
  //
  // Returns true if the given `url` should be blocked.
  static bool ShouldBlockInternal(
      RenderFrameHostImpl* mixed_content_frame,
      FrameTreeNode* node,
      const GURL& url,
      bool for_redirect,
      bool cancel_prerendering,
      blink::mojom::MixedContentContextType mixed_content_context_type,
      std::set<blink::mojom::WebFeature>* mixed_content_features = nullptr,
      bool* should_report_to_renderer = nullptr);

  // Returns the parent frame where mixed content exists for the provided data
  // or nullptr if there is no mixed content.
  //
  // This mirrors `blink::MixedContentChecker::InWhichFrameIsContentMixed()`.
  RenderFrameHostImpl* InWhichFrameIsContentMixed(FrameTreeNode* node,
                                                  const GURL& url);

  FRIEND_TEST_ALL_PREFIXES(MixedContentCheckerTest, IsMixedContent);
  // This mirrors `blink::MixedContentChecker::IsMixedContent()`.
  static bool IsMixedContentForTesting(const GURL& origin_url, const GURL& url);

  // Keeps track of mixed content features encountered while running one of the
  // navigation throttling steps. These values are reported to the respective
  // renderer process after each mixed content check is finished.
  std::set<blink::mojom::WebFeature> navigation_mixed_content_features_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_RENDERER_HOST_MIXED_CONTENT_CHECKER_H_
