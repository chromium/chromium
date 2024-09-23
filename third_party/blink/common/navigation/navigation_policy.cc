// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/navigation_policy.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

NavigationDownloadPolicy::NavigationDownloadPolicy() = default;
NavigationDownloadPolicy::~NavigationDownloadPolicy() = default;
NavigationDownloadPolicy::NavigationDownloadPolicy(
    const NavigationDownloadPolicy&) = default;

void NavigationDownloadPolicy::SetAllowed(NavigationDownloadType type) {
  observed_types.set(static_cast<size_t>(type));
}

void NavigationDownloadPolicy::SetDisallowed(NavigationDownloadType type) {
  observed_types.set(static_cast<size_t>(type));
  disallowed_types.set(static_cast<size_t>(type));
}

bool NavigationDownloadPolicy::IsType(NavigationDownloadType type) const {
  return observed_types.test(static_cast<size_t>(type));
}

ResourceInterceptPolicy NavigationDownloadPolicy::GetResourceInterceptPolicy()
    const {
  if (disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kSandbox)) ||
      disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kOpenerCrossOrigin)) ||
      disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kAdFrame)) ||
      disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kAdFrameNoGesture))) {
    return ResourceInterceptPolicy::kAllowPluginOnly;
  }
  return disallowed_types.any() ? ResourceInterceptPolicy::kAllowNone
                                : ResourceInterceptPolicy::kAllowAll;
}

bool NavigationDownloadPolicy::IsDownloadAllowed() const {
  return disallowed_types.none();
}

void NavigationDownloadPolicy::ApplyDownloadFramePolicy(
    bool is_opener_navigation,
    bool has_gesture,
    bool openee_can_access_opener_origin,
    bool has_download_sandbox_flag,
    bool from_ad) {
  if (!has_gesture)
    SetAllowed(NavigationDownloadType::kNoGesture);

  // Disallow downloads on an opener if the requestor is cross origin.
  // See crbug.com/632514.
  if (is_opener_navigation && !openee_can_access_opener_origin) {
    SetDisallowed(NavigationDownloadType::kOpenerCrossOrigin);
  }

  if (has_download_sandbox_flag) {
    SetDisallowed(NavigationDownloadType::kSandbox);
  }

  if (from_ad) {
    SetAllowed(NavigationDownloadType::kAdFrame);
    if (!has_gesture) {
      SetDisallowed(NavigationDownloadType::kAdFrameNoGesture);
    }
  }
}

blink::mojom::NavigationInitiatorActivationAndAdStatus
GetNavigationInitiatorActivationAndAdStatus(bool has_user_activation,
                                            bool initiator_frame_is_ad,
                                            bool is_ad_script_in_stack) {
  return has_user_activation
             ? ((initiator_frame_is_ad || is_ad_script_in_stack)
                    ? blink::mojom::NavigationInitiatorActivationAndAdStatus::
                          kStartedWithTransientActivationFromAd
                    : blink::mojom::NavigationInitiatorActivationAndAdStatus::
                          kStartedWithTransientActivationFromNonAd)
             : blink::mojom::NavigationInitiatorActivationAndAdStatus::
                   kDidNotStartWithTransientActivation;
}

}  // namespace blink
