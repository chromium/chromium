// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_

#include <bitset>
#include <set>

#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"

namespace content {

// Represents the result whether the page could be stored in the back-forward
// cache with the reasons.
// TODO(rakina): Rename this to use "Page" instead of "Document", to follow
// the naming of BackForwardCacheImpl::CanStorePageNow().
class BackForwardCacheCanStoreDocumentResult {
 public:
  using NotStoredReasons =
      std::bitset<static_cast<size_t>(
                      BackForwardCacheMetrics::NotRestoredReason::kMaxValue) +
                  1ul>;

  BackForwardCacheCanStoreDocumentResult();
  BackForwardCacheCanStoreDocumentResult(
      BackForwardCacheCanStoreDocumentResult&&);
  BackForwardCacheCanStoreDocumentResult& operator=(
      BackForwardCacheCanStoreDocumentResult&&);
  ~BackForwardCacheCanStoreDocumentResult();

  void No(BackForwardCacheMetrics::NotRestoredReason reason);
  void NoDueToFeatures(uint64_t features);
  void NoDueToRelatedActiveContents(base::Optional<ShouldSwapBrowsingInstance>
                                        browsing_instance_not_swapped_reason);

  // TODO(hajimehoshi): Replace the arbitrary strings with base::Location /
  // FROM_HERE for privacy reasons.
  void NoDueToDisableForRenderFrameHostCalled(
      const std::set<std::string>& reasons);

  bool CanStore() const;
  operator bool() const { return CanStore(); }

  const NotStoredReasons& not_stored_reasons() const {
    return not_stored_reasons_;
  }
  uint64_t blocklisted_features() const { return blocklisted_features_; }
  base::Optional<ShouldSwapBrowsingInstance>
  browsing_instance_not_swapped_reason() const {
    return browsing_instance_not_swapped_reason_;
  }
  const std::set<std::string>& disabled_reasons() const {
    return disabled_reasons_;
  }

  std::string ToString() const;

 private:
  std::string NotRestoredReasonToString(
      BackForwardCacheMetrics::NotRestoredReason reason) const;

  NotStoredReasons not_stored_reasons_;
  uint64_t blocklisted_features_ = 0;
  base::Optional<ShouldSwapBrowsingInstance>
      browsing_instance_not_swapped_reason_;
  std::set<std::string> disabled_reasons_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_
