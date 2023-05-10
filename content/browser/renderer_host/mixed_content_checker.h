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

 private:
  // Returns the parent frame where mixed content exists for the provided data
  // or nullptr if there is no mixed content.
  //
  // This mirrors `blink::MixedContentChecker::InWhichFrameIsContentMixed()`.
  RenderFrameHostImpl* InWhichFrameIsContentMixed(FrameTreeNode* node,
                                                  const GURL& url);

  // Updates the renderer about any Blink feature usage.
  void MaybeSendBlinkFeatureUsageReport(NavigationHandle& navigation_handle);

  // Records basic mixed content "feature" usage when any kind of mixed content
  // is found.
  //
  // Based off of `blink::MixedContentChecker::Count()`.
  void ReportBasicMixedContentFeatures(
      blink::mojom::RequestContextType request_context_type,
      blink::mojom::MixedContentContextType mixed_content_context_type);

  FRIEND_TEST_ALL_PREFIXES(MixedContentCheckerTest, IsMixedContent);
  // This mirrors `blink::MixedContentChecker::IsMixedContent()`.
  static bool IsMixedContentForTesting(const GURL& origin_url, const GURL& url);

  // Keeps track of mixed content features encountered while running one of the
  // navigation throttling steps. These values are reported to the respective
  // renderer process after each mixed content check is finished.
  std::set<blink::mojom::WebFeature> mixed_content_features_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_RENDERER_HOST_MIXED_CONTENT_CHECKER_H_
