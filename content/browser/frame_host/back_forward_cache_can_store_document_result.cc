// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/back_forward_cache_can_store_document_result.h"

#include "base/strings/string_util.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

std::string DescribeFeatures(uint64_t blocklisted_features) {
  std::vector<std::string> features;
  for (size_t i = 0;
       i <= static_cast<size_t>(WebSchedulerTrackedFeature::kMaxValue); ++i) {
    if (blocklisted_features & (1 << i)) {
      features.push_back(blink::scheduler::FeatureToString(
          static_cast<WebSchedulerTrackedFeature>(i)));
    }
  }
  return base::JoinString(features, ", ");
}

}  // namespace

bool BackForwardCacheCanStoreDocumentResult::CanStore() const {
  return not_stored_reasons_.none();
}

std::string BackForwardCacheCanStoreDocumentResult::ToString() const {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;

  if (CanStore())
    return "Yes";

  std::vector<std::string> reason_strs;

  for (int i = 0; i <= static_cast<int>(Reason::kMaxValue); i++) {
    if (!not_stored_reasons_.test(static_cast<size_t>(i)))
      continue;

    reason_strs.push_back(NotRestoredReasonToString(static_cast<Reason>(i)));
  }

  return "No: " + base::JoinString(reason_strs, ", ");
}

std::string BackForwardCacheCanStoreDocumentResult::NotRestoredReasonToString(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;

  switch (reason) {
    case Reason::kNotMainFrame:
      return "not a main frame";
    case Reason::kBackForwardCacheDisabled:
      return "BackForwardCache disabled";
    case Reason::kRelatedActiveContentsExist:
      return "related active contents exist";
    case Reason::kHTTPStatusNotOK:
      return "HTTP status is not OK";
    case Reason::kSchemeNotHTTPOrHTTPS:
      return "scheme is not HTTP or HTTPS";
    case Reason::kLoading:
      return "frame is not fully loaded";
    case Reason::kWasGrantedMediaAccess:
      return "frame was granted microphone or camera access";
    case Reason::kBlocklistedFeatures:
      return "blocklisted features: " + DescribeFeatures(blocklisted_features_);
    case Reason::kDisableForRenderFrameHostCalled:
      return "BackForwardCache::DisableForRenderFrameHost() was called";
    case Reason::kDomainNotAllowed:
      return "This domain is not allowed to be stored in BackForwardCache";
    case Reason::kHTTPMethodNotGET:
      return "HTTP method is not GET";
    case Reason::kSubframeIsNavigating:
      return "subframe navigation is in progress";
    case Reason::kTimeout:
      return "timeout";
    case Reason::kCacheLimit:
      return "cache limit";
    case Reason::kJavaScriptExecution:
      return "JavaScript execution";
    case Reason::kRendererProcessKilled:
      return "renderer process is killed";
    case Reason::kRendererProcessCrashed:
      return "renderer process crashed";
    case Reason::kDialog:
      return "dialog";
    case Reason::kGrantedMediaStreamAccess:
      return "granted media stream access";
    case Reason::kSchedulerTrackedFeatureUsed:
      return "scheduler tracked feature is used";
    case Reason::kConflictingBrowsingInstance:
      return "conflicting BrowsingInstance";
    case Reason::kCacheFlushed:
      return "cache flushed";
    case Reason::kServiceWorkerVersionActivation:
      return "service worker version is activated";
  }
}

void BackForwardCacheCanStoreDocumentResult::No(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  not_stored_reasons_.set(static_cast<size_t>(reason));
}

void BackForwardCacheCanStoreDocumentResult::NoDueToFeatures(
    uint64_t features) {
  not_stored_reasons_.set(static_cast<size_t>(
      BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures));
  blocklisted_features_ |= features;
}

BackForwardCacheCanStoreDocumentResult::
    BackForwardCacheCanStoreDocumentResult() = default;
BackForwardCacheCanStoreDocumentResult::BackForwardCacheCanStoreDocumentResult(
    BackForwardCacheCanStoreDocumentResult&&) = default;
BackForwardCacheCanStoreDocumentResult& BackForwardCacheCanStoreDocumentResult::
operator=(BackForwardCacheCanStoreDocumentResult&&) = default;
BackForwardCacheCanStoreDocumentResult::
    ~BackForwardCacheCanStoreDocumentResult() = default;

}  // namespace content
