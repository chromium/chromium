// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PARAMS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PARAMS_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

// Returns true if the |kPrefetchUseContentRefactor| feature is enabled.
bool PrefetchContentRefactorIsEnabled();

// The url of the tunnel proxy.
CONTENT_EXPORT GURL PrefetchProxyHost(const GURL& default_proxy_url);

// The header name used to connect to the tunnel proxy.
std::string PrefetchProxyHeaderKey();

// This value is included in the |PrefetchProxyHeaderKey| request header.
// The tunnel proxy will use this to determine what, if any, experimental
// behavior to apply to requests. If the client is not in any server experiment
// group, this will return an empty string.
std::string PrefetchProxyServerExperimentGroup();

// Returns true if any domain can issue private prefetches using the prefetch
// proxy.
bool PrefetchAllowAllDomains();

// Returns true if any domain can issue private prefetches using the prefetch
// proxy, so long as the user opted-in to extended preloading.
bool PrefetchAllowAllDomainsForExtendedPreloading();

// The maximum number of mainframes allowed to be prefetched at the same time.
int PrefetchServiceMaximumNumberOfConcurrentPrefetches();

// Returns true if an ineligible prefetch request should be put on the network,
// but not cached, to disguise the presence of cookies (or other criteria). The
// return value is randomly decided based on variation params since always
// sending the decoy request is expensive from a data use perspective.
CONTENT_EXPORT bool PrefetchServiceSendDecoyRequestForIneligblePrefetch(
    bool disabled_based_on_user_settings);

// The amount of time to allow a prefetch to take before considering it a
// timeout error.
base::TimeDelta PrefetchTimeoutDuration();

// The maximum body length allowed to be prefetched for mainframe responses in
// bytes.
size_t PrefetchMainframeBodyLengthLimit();

// Whether idle sockets should be closed after every prefetch.
bool PrefetchCloseIdleSockets();

// Whether a spare renderer should be started after all prefetching and NSP is
// complete.
bool PrefetchStartsSpareRenderer();

// The amount of time |PrefetchService| will keep an owned |PrefetchContainer|
// alive. If this value is zero or less, the service will keep the prefetch
// forever.
base::TimeDelta PrefetchContainerLifetimeInPrefetchService();

// Retrieves a host for which the prefetch proxy should be bypassed for testing
// purposes.
CONTENT_EXPORT absl::optional<std::string> PrefetchBypassProxyForHost();

// Whether only prefetched resources with a text/html MIME type should be used.
// If this is false, there is no MIME type restriction.
bool PrefetchServiceHTMLOnly();

// The maximum time a prefetched response is servable.
CONTENT_EXPORT base::TimeDelta PrefetchCacheableDuration();

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PARAMS_H_
