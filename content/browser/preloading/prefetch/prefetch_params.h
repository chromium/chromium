// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PARAMS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PARAMS_H_

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"
#include "url/gurl.h"

namespace content {

// The url of the tunnel proxy.
CONTENT_EXPORT GURL PrefetchProxyHost(const GURL& default_proxy_url);

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

// Whether a spare renderer should be started after prefetching.
bool PrefetchStartsSpareRenderer();

// The amount of time |PrefetchService| will keep an owned |PrefetchContainer|
// alive. If this value is zero or less, the service will keep the prefetch
// forever.
base::TimeDelta PrefetchContainerLifetimeInPrefetchService();

// Returns if the specified host should have the prefetch proxy bypassed for
// testing purposes. Currently this is only used for WPT test servers.
CONTENT_EXPORT bool ShouldPrefetchBypassProxyForTestHost(std::string_view host);

// Whether only prefetched resources with a text/html MIME type should be used.
// If this is false, there is no MIME type restriction.
bool PrefetchServiceHTMLOnly();

// The maximum time a prefetched response is servable.
CONTENT_EXPORT base::TimeDelta PrefetchCacheableDuration();

// Whether probing must be done at all.
bool PrefetchProbingEnabled();

// Whether an ISP filtering canary check should be made on browser startup.
bool PrefetchCanaryCheckEnabled();

// Whether the TLS ISP filtering canary check should enabled. Only has effect if
// canary checks are enabled (PrefetchProxyCanaryCheckEnabled is true). When
// false, only the DNS canary check will be performed. When true, both the DNS
// and TLS canary checks will be enabled.
bool PrefetchTLSCanaryCheckEnabled();

// The URL to use for the TLS canary check.
GURL PrefetchTLSCanaryCheckURL(const GURL& default_tls_canary_check_url);

// The URL to use for the DNS canary check.
GURL PrefetchDNSCanaryCheckURL(const GURL& default_dns_canary_check_url);

// How long a canary check can be cached for the same network.
base::TimeDelta PrefetchCanaryCheckCacheLifetime();

// The amount of time to allow before timing out a canary check.
base::TimeDelta PrefetchCanaryCheckTimeout();

// The number of retries to allow for canary checks.
int PrefetchCanaryCheckRetries();

// The maximum amount of time to block until the head of a prefetch is received.
// If the value is zero or less, then a navigation can be blocked indefinitely.
CONTENT_EXPORT base::TimeDelta PrefetchBlockUntilHeadTimeout(
    const PrefetchType& prefetch_type);

// Gets the histogram suffix to use for the given eagerness parameter.
CONTENT_EXPORT std::string GetPrefetchEagernessHistogramSuffix(
    blink::mojom::SpeculationEagerness eagerness);

// Returns the max number of eager prefetches allowed.
size_t MaxNumberOfEagerPrefetchesPerPage();
// Returns the max number of non-eager prefetches allowed.
size_t MaxNumberOfNonEagerPrefetchesPerPage();

// Returns true if NIK prefetch scope is enabled. See crbug.com/1502326
bool PrefetchNIKScopeEnabled();

// Returns true if browser-initiated prefetch is enabled.
// Please see crbug.com/40946257 for more details.
bool PrefetchBrowserInitiatedTriggersEnabled();

// Returns true iff prefetch code should use new wait loop in
// `PrefetchMatchResolver2::FindPrefetch()` instead of
// `PrefetchService::GetPrefetchToServe()`.
CONTENT_EXPORT bool UseNewWaitLoop();

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PARAMS_H_
