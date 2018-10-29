// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/appcache/appcache_frontend_impl.h"

#include "base/logging.h"
#include "content/renderer/appcache/web_application_cache_host_impl.h"
#include "third_party/blink/public/web/web_console_message.h"

using blink::WebApplicationCacheHost;
using blink::WebConsoleMessage;

namespace content {

// Inline helper to keep the lines shorter and unwrapped.
inline WebApplicationCacheHostImpl* GetHost(int id) {
  return WebApplicationCacheHostImpl::FromId(id);
}

void AppCacheFrontendImpl::OnCacheSelected(int host_id,
                                           const AppCacheInfo& info) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnCacheSelected(info);
}

void AppCacheFrontendImpl::OnStatusChanged(const std::vector<int>& host_ids,
                                           AppCacheStatus status) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnStatusChanged(status);
  }
}

void AppCacheFrontendImpl::OnEventRaised(const std::vector<int>& host_ids,
                                         AppCacheEventID event_id) {
  DCHECK_NE(
      event_id,
      AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK_NE(event_id,
            AppCacheEventID::APPCACHE_ERROR_EVENT);  // See OnErrorEventRaised.
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnEventRaised(event_id);
  }
}

void AppCacheFrontendImpl::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url,
    int num_total,
    int num_complete) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnProgressEventRaised(url, num_total, num_complete);
  }
}

void AppCacheFrontendImpl::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const AppCacheErrorDetails& details) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnErrorEventRaised(details);
  }
}

void AppCacheFrontendImpl::OnLogMessage(int host_id,
                                        AppCacheLogLevel log_level,
                                        const std::string& message) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnLogMessage(log_level, message);
}

void AppCacheFrontendImpl::OnContentBlocked(int host_id,
                                            const GURL& manifest_url) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnContentBlocked(manifest_url);
}

void AppCacheFrontendImpl::OnSetSubresourceFactory(
    int host_id,
    network::mojom::URLLoaderFactoryPtr url_loader_factory) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->SetSubresourceFactory(std::move(url_loader_factory));
}

// Ensure that enum values never get out of sync with the
// ones declared for use within the WebKit api

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatched enum: " #a)

STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUncached,
                   AppCacheStatus::APPCACHE_STATUS_UNCACHED);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kIdle,
                   AppCacheStatus::APPCACHE_STATUS_IDLE);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kChecking,
                   AppCacheStatus::APPCACHE_STATUS_CHECKING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kDownloading,
                   AppCacheStatus::APPCACHE_STATUS_DOWNLOADING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUpdateReady,
                   AppCacheStatus::APPCACHE_STATUS_UPDATE_READY);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kObsolete,
                   AppCacheStatus::APPCACHE_STATUS_OBSOLETE);

STATIC_ASSERT_ENUM(WebApplicationCacheHost::kCheckingEvent,
                   AppCacheEventID::APPCACHE_CHECKING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kErrorEvent,
                   AppCacheEventID::APPCACHE_ERROR_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kNoUpdateEvent,
                   AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kDownloadingEvent,
                   AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kProgressEvent,
                   AppCacheEventID::APPCACHE_PROGRESS_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUpdateReadyEvent,
                   AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kCachedEvent,
                   AppCacheEventID::APPCACHE_CACHED_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kObsoleteEvent,
                   AppCacheEventID::APPCACHE_OBSOLETE_EVENT);

STATIC_ASSERT_ENUM(WebApplicationCacheHost::kManifestError,
                   AppCacheErrorReason::APPCACHE_MANIFEST_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kSignatureError,
                   AppCacheErrorReason::APPCACHE_SIGNATURE_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kResourceError,
                   AppCacheErrorReason::APPCACHE_RESOURCE_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kChangedError,
                   AppCacheErrorReason::APPCACHE_CHANGED_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kAbortError,
                   AppCacheErrorReason::APPCACHE_ABORT_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kQuotaError,
                   AppCacheErrorReason::APPCACHE_QUOTA_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kPolicyError,
                   AppCacheErrorReason::APPCACHE_POLICY_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUnknownError,
                   AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR);

STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelVerbose, APPCACHE_LOG_VERBOSE);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelInfo, APPCACHE_LOG_INFO);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelWarning, APPCACHE_LOG_WARNING);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelError, APPCACHE_LOG_ERROR);

#undef STATIC_ASSERT_ENUM

}  // namespace content
