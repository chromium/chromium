// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_NAVIGATION_POLICY_H_
#define CONTENT_PUBLIC_COMMON_NAVIGATION_POLICY_H_

#include <bitset>

#include "content/common/content_export.h"
#include "content/public/common/resource_intercept_policy.h"

// A centralized file for base helper methods and policy decisions about
// navigations.

namespace content {
// Navigation type that affects the download decision and relevant metrics to be
// reported at download-discovery time.
//
// This enum backs a histogram. Please keep enums.xml up to date with any
// changes, and new entries should be appended at the end. Never re-arrange /
// re-use values.
enum class NavigationDownloadType {
  // An entry reserved just for histogram. The client code is not expected to
  // set or query this type in a policy.
  kDefaultAllow = 0,

  kViewSource = 1,
  kInterstitial = 2,

  // The navigation was initiated on a x-origin opener.
  kOpenerCrossOrigin = 5,

  // The navigation was initiated from or occurred in an ad frame without user
  // activation.
  kAdFrameNoGesture = 8,

  // The navigation was initiated from or occurred in an ad frame.
  kAdFrame = 10,

  // The navigation was initiated from or occurred in an iframe with
  // |network::mojom::WebSandboxFlags::kDownloads| flag set.
  kSandbox = 11,

  // The navigation was initiated without user activation.
  kNoGesture = 12,

  kMaxValue = kNoGesture
};

// Stores the navigation types that may be of interest to the download-related
// metrics to be reported at download-discovery time. Also controls how
// navigations behave when they turn into downloads. By default, navigation is
// allowed to become a download.
struct CONTENT_EXPORT NavigationDownloadPolicy {
  NavigationDownloadPolicy();
  ~NavigationDownloadPolicy();
  NavigationDownloadPolicy(const NavigationDownloadPolicy&);

  // Stores |type| to |observed_types|.
  void SetAllowed(NavigationDownloadType type);

  // Stores |type| to both |observed_types| and |disallowed_types|.
  void SetDisallowed(NavigationDownloadType type);

  // Returns if |observed_types| contains |type|.
  bool IsType(NavigationDownloadType type) const;

  // Get the ResourceInterceptPolicy derived from |disallowed_types|.
  ResourceInterceptPolicy GetResourceInterceptPolicy() const;

  // Returns if download is allowed based on |disallowed_types|.
  bool IsDownloadAllowed() const;

  // Record the download policy to histograms from |observed_types|.
  void RecordHistogram() const;

  // A bitset of navigation types observed that may be of interest to the
  // download-related metrics to be reported at download-discovery time.
  std::bitset<static_cast<size_t>(NavigationDownloadType::kMaxValue) + 1>
      observed_types;

  // A bitset of navigation types observed where if the navigation turns into
  // a download, the download should be dropped.
  std::bitset<static_cast<size_t>(NavigationDownloadType::kMaxValue) + 1>
      disallowed_types;

  bool blocking_downloads_in_sandbox_enabled = false;
};

// If this is false we continue the old behaviour of doing an early call to
// RenderFrameHostManager::CommitPending when we are replacing a crashed
// frame.
// TODO(https://crbug.com/1072817): Stop allowing this.
CONTENT_EXPORT bool ShouldSkipEarlyCommitPendingForCrashedFrame();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_NAVIGATION_POLICY_H_
