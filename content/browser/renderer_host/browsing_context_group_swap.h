// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_GROUP_SWAP_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_GROUP_SWAP_H_

#include <optional>

#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/common/content_export.h"

namespace content {

// This enum describes the different type of decisions we can take regarding
// swapping browsing context group during a navigation.
enum class BrowsingContextGroupSwapType {
  // Used when no swap is required.
  kNoSwap,
  // Used for swaps forced by a non matching COOP policy.
  kCoopSwap,
  // Used for swaps forced by a non-COOP security reason. This could be a
  // navigation from a WebUI page to a normal page for example.
  kSecuritySwap,
  // Used for swaps that occur when not strictly required, to support the
  // BackForwardCache.
  kProactiveSwap
};

// This class represents the decision taken regarding a browsing context group
// swap. It is created via one of the static members depending on the actual
// case. The underlying consequences of that decision can be computed via
// simple getters.
class CONTENT_EXPORT BrowsingContextGroupSwap {
 public:
  static BrowsingContextGroupSwap CreateDefault();
  static BrowsingContextGroupSwap CreateNoSwap(
      ShouldSwapBrowsingInstance reason);
  static BrowsingContextGroupSwap CreateCoopSwap();
  static BrowsingContextGroupSwap CreateSecuritySwap();
  static BrowsingContextGroupSwap CreateProactiveSwap(
      ShouldSwapBrowsingInstance reason);

  BrowsingContextGroupSwapType type() const { return type_; }
  ShouldSwapBrowsingInstance reason() const { return reason_.value(); }

  // Returns whether we should use a different browsing context group for the
  // navigation.
  bool ShouldSwap() const;

  // Indicates whether the proxies to other documents in this browsing context
  // group should be cleared before moving eventually going to a new group. This
  // is not the case for all swaps, for compatibility reasons.
  // See https://crbug.com/1366827 for details.
  bool ShouldClearProxiesOnCommit() const;

  // Whether or not we should clear the window's name upon navigation.
  bool ShouldClearWindowName() const;

 private:
  BrowsingContextGroupSwap(
      BrowsingContextGroupSwapType type,
      const std::optional<ShouldSwapBrowsingInstance>& reason);

  // Describes the type of browsing context group swap we've decided to make.
  BrowsingContextGroupSwapType type_;

  // Describes the reason why we've taken that decision in terms understandable
  // by the BackForwardCache metrics. This is null if created using the
  // `CreateDefault()` method.
  std::optional<ShouldSwapBrowsingInstance> reason_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_GROUP_SWAP_H_
