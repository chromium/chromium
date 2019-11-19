// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_MIXED_CONTENT_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_FRAME_HOST_MIXED_CONTENT_NAVIGATION_THROTTLE_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"

namespace content {

class FrameTreeNode;

// Responsible for browser-process-side mixed content security checks. It checks
// only for frame-level resource loads (aka navigation loads). Sub-resources
// fetches are checked in the renderer process by MixedContentChecker. Changes
// to this class might need to be reflected on its renderer counterpart.
//
// Current mixed content W3C draft that drives this implementation:
// https://w3c.github.io/webappsec-mixed-content/
class MixedContentNavigationThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);

  MixedContentNavigationThrottle(NavigationHandle* navigation_handle);
  ~MixedContentNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MixedContentNavigationThrottleTest, IsMixedContent);

  // Checks if a navigation should be blocked or not due to mixed content.
  bool ShouldBlockNavigation(bool for_redirect);

  // Returns the parent frame where mixed content exists for the provided data
  // or nullptr if there is no mixed content.
  FrameTreeNode* InWhichFrameIsContentMixed(FrameTreeNode* node,
                                            const GURL& url);

  // Updates the renderer about any Blink feature usage.
  void MaybeSendBlinkFeatureUsageReport();

  // Records basic mixed content "feature" usage when any kind of mixed content
  // is found.
  void ReportBasicMixedContentFeatures(
      blink::mojom::RequestContextType request_context_type,
      blink::WebMixedContentContextType mixed_content_context_type);

  static bool CONTENT_EXPORT IsMixedContentForTesting(const GURL& origin_url,
                                                      const GURL& url);

  // Keeps track of mixed content features encountered while running one of the
  // navigation throttling steps. These values are reported to the respective
  // renderer process after each mixed content check is finished.
  std::set<blink::mojom::WebFeature> mixed_content_features_;

  DISALLOW_COPY_AND_ASSIGN(MixedContentNavigationThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_MIXED_CONTENT_NAVIGATION_THROTTLE_H_
