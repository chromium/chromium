// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_group_swap.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace content {

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateDefault() {
  return {BrowsingContextGroupSwapType::kNoSwap, std::nullopt};
}

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateNoSwap(
    ShouldSwapBrowsingInstance no_swap_reason) {
  return {BrowsingContextGroupSwapType::kNoSwap, no_swap_reason};
}

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateCoopSwap() {
  return {BrowsingContextGroupSwapType::kCoopSwap,
          ShouldSwapBrowsingInstance::kYes_ForceSwap};
}

BrowsingContextGroupSwap BrowsingContextGroupSwap::CreateRelatedCoopSwap() {
  return {BrowsingContextGroupSwapType::kRelatedCoopSwap,
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
    case BrowsingContextGroupSwapType::kRelatedCoopSwap:
    case BrowsingContextGroupSwapType::kSecuritySwap:
    case BrowsingContextGroupSwapType::kProactiveSwap:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool BrowsingContextGroupSwap::ShouldClearProxiesOnCommit() const {
  switch (type_) {
    case BrowsingContextGroupSwapType::kNoSwap:
    case BrowsingContextGroupSwapType::kSecuritySwap:
    case BrowsingContextGroupSwapType::kProactiveSwap:
    case BrowsingContextGroupSwapType::kRelatedCoopSwap:
      return false;

    case BrowsingContextGroupSwapType::kCoopSwap:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool BrowsingContextGroupSwap::ShouldClearWindowName() const {
  switch (type_) {
    case BrowsingContextGroupSwapType::kNoSwap:
    case BrowsingContextGroupSwapType::kSecuritySwap:
    case BrowsingContextGroupSwapType::kProactiveSwap:
      return false;

    case BrowsingContextGroupSwapType::kCoopSwap:
    case BrowsingContextGroupSwapType::kRelatedCoopSwap:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

BrowsingContextGroupSwap::BrowsingContextGroupSwap(
    BrowsingContextGroupSwapType type,
    const std::optional<ShouldSwapBrowsingInstance>& reason)
    : type_(type), reason_(reason) {}

}  // namespace content
