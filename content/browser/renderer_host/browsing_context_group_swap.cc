// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_group_swap.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace content {

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateDefault() {
  return {BrowsingContextGroupSwapType::kNoSwap, absl::nullopt};
}

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateNoSwap(
    ShouldSwapBrowsingInstance no_swap_reason) {
  return {BrowsingContextGroupSwapType::kNoSwap, no_swap_reason};
}

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateCoopSwap() {
  return {BrowsingContextGroupSwapType::kCoopSwap,
          ShouldSwapBrowsingInstance::kYes_ForceSwap};
}

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateSecuritySwap() {
  return {BrowsingContextGroupSwapType::kSecuritySwap,
          ShouldSwapBrowsingInstance::kYes_ForceSwap};
}

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateProactiveSwap(
    ShouldSwapBrowsingInstance should_swap_reason) {
  return {BrowsingContextGroupSwapType::kProactiveSwap, should_swap_reason};
}

bool BrowsingContextGroupSwap::ShouldSwap() const {
  switch (type_) {
    case BrowsingContextGroupSwapType::kNoSwap:
      return false;

    case BrowsingContextGroupSwapType::kCoopSwap:
    case BrowsingContextGroupSwapType::kSecuritySwap:
    case BrowsingContextGroupSwapType::kProactiveSwap:
      return true;
  }
  NOTREACHED();
  return false;
}

bool BrowsingContextGroupSwap::ShouldClearProxiesOnCommit() const {
  switch (type_) {
    case BrowsingContextGroupSwapType::kNoSwap:
    case BrowsingContextGroupSwapType::kSecuritySwap:
    case BrowsingContextGroupSwapType::kProactiveSwap:
      return false;

    case BrowsingContextGroupSwapType::kCoopSwap:
      return true;
  }
  NOTREACHED();
  return false;
}

bool BrowsingContextGroupSwap::ShouldClearWindowName() const {
  switch (type_) {
    case BrowsingContextGroupSwapType::kNoSwap:
    case BrowsingContextGroupSwapType::kSecuritySwap:
    case BrowsingContextGroupSwapType::kProactiveSwap:
      return false;

    case BrowsingContextGroupSwapType::kCoopSwap:
      return true;
  }
  NOTREACHED();
  return false;
}

BrowsingContextGroupSwap::BrowsingContextGroupSwap(
    BrowsingContextGroupSwapType type,
    const absl::optional<ShouldSwapBrowsingInstance>& reason)
    : type_(type), reason_(reason) {}

}  // namespace content
