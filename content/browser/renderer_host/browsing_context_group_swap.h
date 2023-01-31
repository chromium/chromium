// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_GROUP_SWAP_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_GROUP_SWAP_H_

#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This enum describes the different type of decisions we can take regarding
// swapping BrowsingContext group during a navigation.
enum class BrowsingContextGroupSwapType {
  // Used when no swap is required.
  kNoSwap,
  // Used for swaps forced by a non matching COOP policy.
  kCoopSwap,
  // Used for some swaps forced by a non matching COOP: restrict-properties
  // policy. It puts the new document into a related BrowsingContext group.
  //
  // Contrary to unrelated BrowsingContext groups, the communication in between
  // two related BrowsingContext groups is possible, but limited to using
  // Window.postMessage() and Window.closed only.
  kRelatedCoopSwap,
  // Used for swaps forced by a non-COOP security reason. This could be a
  // navigation from a WebUI page to a normal page for example.
  kSecuritySwap,
  // Used for swaps that occur when not strictly required, to support the
  // BackForwardCache.
  kProactiveSwap
};

// This class represents the decision taken regarding a BrowsingContext group
// swap. It is created via one of the static members depending on the actual
// case. The underlying consequences of that decision can be computed via
// simple getters.
class CONTENT_EXPORT BrowsingContextGroupSwap {
 public:
  static BrowsingContextGroupSwap CreateDefault();
  static BrowsingContextGroupSwap CreateNoSwap(
      ShouldSwapBrowsingInstance reason);
  static BrowsingContextGroupSwap CreateCoopSwap();
  static BrowsingContextGroupSwap CreateRelatedCoopSwap();
  static BrowsingContextGroupSwap CreateSecuritySwap();
  static BrowsingContextGroupSwap CreateProactiveSwap(
      ShouldSwapBrowsingInstance reason);

  BrowsingContextGroupSwapType type() const { return type_; }
  ShouldSwapBrowsingInstance reason() const { return reason_.value(); }
  bool ShouldSwap() const;
  bool ShouldClearProxiesOnCommit() const;
  bool ShouldClearWindowName() const;

 private:
  BrowsingContextGroupSwap(
      BrowsingContextGroupSwapType type,
      const absl::optional<ShouldSwapBrowsingInstance>& reason);

  // Describes the type of BrowsingContext group swap we've decided to make.
  BrowsingContextGroupSwapType type_;

  // Describes the reason why we've taken that decision in terms understandable
  // by the BackForwardCache metrics. This is null if created using the
  // `CreateDefault()` method.
  absl::optional<ShouldSwapBrowsingInstance> reason_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_GROUP_SWAP_H_
