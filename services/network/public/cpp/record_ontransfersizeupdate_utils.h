// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RECORD_ONTRANSFERSIZEUPDATE_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RECORD_ONTRANSFERSIZEUPDATE_UTILS_H_

#include "base/metrics/histogram_macros.h"

namespace network {

enum class OnTransferSizeUpdatedFrom {
  kCacheAliasSearchPrefetchURLLoader = 0,
  kCorsURLLoader = 1,
  kDelegatingURLLoaderClient = 2,
  kDownloadResponseHandler = 3,
  kDriveFsURLLoaderClient = 4,
  kEmptyURLLoaderClient = 5,
  kFakeEmbeddedWorkerInstanceClient = 6,
  kHeaderRewritingURLLoaderClient = 7,
  kInterceptedRequest = 8,
  kInterceptionJob = 9,
  kMimeSniffingURLLoader = 10,
  kMojoURLLoaderClient = 11,
  kNavigationBodyLoader = 12,
  kNavigationPreloadRequest = 13,
  kNavigationURLLoaderImpl = 14,
  kObjectNavigationFallbackBodyLoader = 15,
  kPrefetchProxyProxyingURLLoaderFactory = 16,
  kPrefetchURLLoader = 17,
  kPreloadURLLoaderClient = 18,
  kProxyingURLLoaderFactory = 19,
  kResultRecordingClient = 20,
  kServiceWorkerNewScriptFetcher = 21,
  kServiceWorkerSingleScriptUpdateChecker = 22,
  kSignedExchangeCertFetcher = 23,
  kSignedExchangeLoader = 24,
  kSignedExchangePrefetchHandler = 25,
  kSimpleURLLoaderImpl = 26,
  kStreamingSearchPrefetchURLLoader = 27,
  kThrottlingURLLoader = 28,
  kURLLoaderClientCheckedRemote = 29,
  kURLLoaderRelay = 30,
  kURLLoaderStatusMonitor = 31,
  kWebBundleURLLoaderClient = 32,
  kWebRequestProxyingURLLoaderFactory = 33,
  kWorkerMainScriptLoader = 34,
  kWorkerScriptFetcher = 35,
  kWorkerScriptLoader = 36,
  kServiceWorkerRaceNetworkRequest = 37,
  kMaxValue = kServiceWorkerRaceNetworkRequest,
};

inline void RecordOnTransferSizeUpdatedUMA(
    OnTransferSizeUpdatedFrom class_name) {
  UMA_HISTOGRAM_ENUMERATION(
      "Net.OnTransferSizeUpdated.Experimental.OverridenBy", class_name);
}

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RECORD_ONTRANSFERSIZEUPDATE_UTILS_H_
