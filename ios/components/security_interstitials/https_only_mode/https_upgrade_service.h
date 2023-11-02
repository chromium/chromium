// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_SERVICE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_SERVICE_H_

#include <string>

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

// HttpsUpgradeService tracks the allowlist decisions for HTTPS-Only mode.
// Decisions are scoped to the host.
class HttpsUpgradeService : public KeyedService {
 public:
  // Returns whether `host` can be loaded over http://.
  virtual bool IsHttpAllowedForHost(const std::string& host) const = 0;

  // Allows future navigations to `host` over http://.
  virtual void AllowHttpForHost(const std::string& host) = 0;

  // Clears the persistent and in-memory allowlist entries. All of in-memory
  // entries are removed, but only persistent entries between delete_begin and
  // delete_end are removed.
  virtual void ClearAllowlist(base::Time delete_begin,
                              base::Time delete_end) = 0;

  // Sets the HTTPS port used by the embedded https server. This is used to
  // determine the correct port while upgrading URLs to https if the original
  // URL has a non-default port. If use_fake_https_for_testing is true, the
  // tests are using an HTTP server that pretends to serve HTTPS responses.
  void SetHttpsPortForTesting(int https_port_for_testing,
                              bool use_fake_https_for_testing);
  void SetFallbackHttpPortForTesting(int http_port_for_testing);

  // Returns the HTTPS port used by the embedded test server (real or fake).
  int GetHttpsPortForTesting() const;
  // Returns true if the tests are using a fake HTTPS server that's actually an
  // HTTP server.
  bool IsUsingFakeHttpsForTesting() const;

  // Returns true if url is a fake HTTPS URL used in tests. Tests use a fake
  // HTTPS server that actually serves HTTP but on a different port from the
  // test HTTP server. We shouldn't upgrade HTTP URLs from from the fake HTTPS
  // server.
  bool IsFakeHTTPSForTesting(const GURL& url) const;

  // Returns true if the url is a localhost URL, taking tests into account. The
  // test server serves content from an IP address instead of a hostname, so
  // this function will return false for IP addresses in tests.
  bool IsLocalhost(const GURL& url) const;

  // Returns the upgraded HTTPS version of the given url.
  GURL GetUpgradedHttpsUrl(const GURL& http_url) const;
  GURL GetHttpUrl(const GURL& url) const;

  // Returns the delay before a fallback navigation is started for a slow HTTPS
  // load.
  base::TimeDelta GetFallbackDelay() const;
  // Sets the fallback delay for tests. If set to zero, a fallback navigation
  // will be immediately started after the navigation is upgraded to HTTPS.
  void SetFallbackDelayForTesting(base::TimeDelta delay);

 private:
  int https_port_for_testing_ = 0;
  int http_port_for_testing_ = 0;
  bool use_fake_https_for_testing_ = false;

  // Delay before a fallback navigation is initiated.
  base::TimeDelta fallback_delay_ = base::Seconds(3);
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_SERVICE_H_
