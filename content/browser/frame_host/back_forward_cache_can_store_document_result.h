// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_
#define CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_

#include <bitset>

#include "content/browser/frame_host/back_forward_cache_metrics.h"

namespace content {

// Represents the result whether the page could be stored in the back-forward
// cache with the reasons.
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

  bool CanStore() const;
  operator bool() const { return CanStore(); }

  const NotStoredReasons& not_stored_reasons() const {
    return not_stored_reasons_;
  }
  uint64_t blocklisted_features() const { return blocklisted_features_; }

  std::string ToString() const;

 private:
  std::string NotRestoredReasonToString(
      BackForwardCacheMetrics::NotRestoredReason reason) const;

  NotStoredReasons not_stored_reasons_;
  uint64_t blocklisted_features_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_
