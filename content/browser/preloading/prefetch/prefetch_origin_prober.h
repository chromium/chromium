// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_ORIGIN_PROBER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_ORIGIN_PROBER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/common/content_export.h"
#include "net/base/address_list.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class PrefetchCanaryChecker;

// This class handles all probing and canary checks for the prefetch proxy
// feature. Calling code should use |ShouldProbeOrigins| to determine if a probe
// is needed before using prefetched resources and if so, call |Probe|. See
// http://crbug.com/1109992 for more details.
class CONTENT_EXPORT PrefetchOriginProber {
 public:
  PrefetchOriginProber(BrowserContext* browser_context,
                       const GURL& dns_canary_check_url,
                       const GURL& tls_canary_check_url);
  virtual ~PrefetchOriginProber();

  PrefetchOriginProber(const PrefetchOriginProber&) = delete;
  PrefetchOriginProber& operator=(const PrefetchOriginProber) = delete;

  // Run canary checks if they are not already cached.
  void RunCanaryChecksIfNeeded() const;

  // Returns true if a probe needs to be done before using prefetched resources.
  virtual bool ShouldProbeOrigins() const;

  // Starts a probe to |url| and calls |callback| with an
  // |PrefetchProbeResult| to indicate success.
  using OnProbeResultCallback = base::OnceCallback<void(PrefetchProbeResult)>;
  virtual void Probe(const GURL& url, OnProbeResultCallback callback);

 private:
  // Does a DNS resolution for a DNS or TLS probe, passing all the arguments to
  // |OnDNSResolved|.
  void StartDNSResolution(const GURL& url,
                          OnProbeResultCallback callback,
                          bool also_do_tls_connect);

  // If the DNS resolution was successful, this will either run |callback| for a
  // DNS probe, or start the TLS socket for a TLS probe. This is determined by
  // |also_do_tls_connect|. If the DNS resolution failed, |callback| is run with
  // failure.
  void OnDNSResolved(const GURL& url,
                     OnProbeResultCallback callback,
                     bool also_do_tls_connect,
                     int net_error,
                     const std::optional<net::AddressList>& resolved_addresses);

  // Both DNS and TLS probes need to resolve DNS. This starts the TLS probe with
  // the |addresses| from the DNS resolution.
  void DoTLSProbeAfterDNSResolution(const GURL& url,
                                    OnProbeResultCallback callback,
                                    const net::AddressList& addresses);

  // The current browser context, not owned.
  raw_ptr<BrowserContext> browser_context_;

  // The TLS canary url checker.
  std::unique_ptr<PrefetchCanaryChecker> tls_canary_checker_;

  // The DNS canary url checker.
  std::unique_ptr<PrefetchCanaryChecker> dns_canary_checker_;

  base::WeakPtrFactory<PrefetchOriginProber> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_ORIGIN_PROBER_H_
