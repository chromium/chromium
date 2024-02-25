// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_SERVICE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_SERVICE_DELEGATE_H_

#include <string>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/preloading.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class WebContents;

// Allows embedders to control certain aspects of |PrefetchService|.
class CONTENT_EXPORT PrefetchServiceDelegate {
 public:
  virtual ~PrefetchServiceDelegate() = default;

  // Clears data for delegate associated with given |browser_context|.
  static void ClearData(BrowserContext* browser_context);

  // Gets the major version of the embedder. Only the major version of embedder
  // is included in the user agent for prefetch requests.
  virtual std::string GetMajorVersionNumber() = 0;

  // Gets the accept language header to be included in prefetch requests.
  virtual std::string GetAcceptLanguageHeader() = 0;

  // Gets the default host of the prefetch proxy.
  virtual GURL GetDefaultPrefetchProxyHost() = 0;

  // Gets API key for making prefetching requests.
  virtual std::string GetAPIKey() = 0;

  // Gets the default URLs used in the DNS and TLS canary checks.
  virtual GURL GetDefaultDNSCanaryCheckURL() = 0;
  virtual GURL GetDefaultTLSCanaryCheckURL() = 0;

  // Reports that a 503 response with a "Retry-After" header was received from
  // |url|. Indicates that we shouldn't send new prefetch requests to that
  // origin for |retry_after| amount of time.
  virtual void ReportOriginRetryAfter(const GURL& url,
                                      base::TimeDelta retry_after) = 0;

  // Returns whether or not the URL is eligible for prefetch based on previous
  // responses with a "Retry-After" header.
  virtual bool IsOriginOutsideRetryAfterWindow(const GURL& url) = 0;

  // Clears any browsing data associated with the delegate, specifically any
  // information about "Retry-Afters" received.
  virtual void ClearData() = 0;

  // Checks if we can disable sending decoy prefetches based on the user's
  // settings.
  virtual bool DisableDecoysBasedOnUserSettings() = 0;

  // Get the state of the user's preloading settings.
  virtual PreloadingEligibility IsSomePreloadingEnabled() = 0;
  virtual bool IsExtendedPreloadingEnabled() = 0;
  virtual bool IsPreloadingPrefEnabled() = 0;
  virtual bool IsDataSaverEnabled() = 0;
  virtual bool IsBatterySaverEnabled() = 0;

  // Checks if the referring page is in the allow list to make prefetches.
  virtual bool IsDomainInPrefetchAllowList(const GURL& referring_url) = 0;

  // Determines whether a referring URL is reasonably trusted to proceed without
  // delay when processing cross-site prefetches.
  virtual bool IsContaminationExempt(const GURL& referring_url) = 0;

  virtual void OnPrefetchLikely(WebContents* web_contents) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_SERVICE_DELEGATE_H_
