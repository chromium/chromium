// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_NAVIGATION_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_NAVIGATION_POLICY_H_

#include <bitset>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/navigation/resource_intercept_policy.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"

// A centralized file for base helper methods and policy decisions about
// navigations.

namespace blink {
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
struct BLINK_COMMON_EXPORT NavigationDownloadPolicy {
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

  // Possibly set the kOpenerCrossOrigin and kSandboxNoGesture policy in
  // |download_policy|. The parameter `openee_can_access_opener_origin` only
  // matters if `is_opener_navigation` is true.
  void ApplyDownloadFramePolicy(bool is_opener_navigation,
                                bool has_gesture,
                                bool openee_can_access_opener_origin,
                                bool has_download_sandbox_flag,
                                bool from_ad);

  // An alias of a bitset of navigation types.
  using NavigationDownloadTypes =
      std::bitset<static_cast<size_t>(NavigationDownloadType::kMaxValue) + 1>;

  // A bitset of navigation types observed that may be of interest to the
  // download-related metrics to be reported at download-discovery time.
  NavigationDownloadTypes observed_types;

  // A bitset of navigation types observed where if the navigation turns into
  // a download, the download should be dropped.
  NavigationDownloadTypes disallowed_types;
};

// Construct a `NavigationInitiatorActivationAndAdStatus` based on the user
// activation and ad status.
BLINK_COMMON_EXPORT
blink::mojom::NavigationInitiatorActivationAndAdStatus
GetNavigationInitiatorActivationAndAdStatus(bool has_user_activation,
                                            bool is_ad_script_in_stack);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_NAVIGATION_POLICY_H_
