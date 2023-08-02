// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"

#import "net/base/url_util.h"

void HttpsUpgradeService::SetHttpsPortForTesting(
    int https_port_for_testing,
    bool use_fake_https_for_testing) {
  https_port_for_testing_ = https_port_for_testing;
  use_fake_https_for_testing_ = use_fake_https_for_testing;
}

int HttpsUpgradeService::GetHttpsPortForTesting() const {
  return https_port_for_testing_;
}

bool HttpsUpgradeService::IsUsingFakeHttpsForTesting() const {
  return use_fake_https_for_testing_;
}

bool HttpsUpgradeService::IsFakeHTTPSForTesting(const GURL& url) const {
  return url.IntPort() == https_port_for_testing_;
}

bool HttpsUpgradeService::IsLocalhost(const GURL& url) const {
  // Tests use 127.0.0.1 for embedded servers, which is a localhost URL.
  // Only check for "localhost" in tests.
  if (https_port_for_testing_) {
    return url.host() == "localhost";
  }
  return net::IsLocalhost(url);
}

void HttpsUpgradeService::SetFallbackHttpPortForTesting(int http_port) {
  http_port_for_testing_ = http_port;
}

GURL HttpsUpgradeService::GetUpgradedHttpsUrl(const GURL& http_url) const {
  DCHECK_EQ(url::kHttpScheme, http_url.scheme());
  GURL::Replacements replacements;

  // This needs to be in scope when ReplaceComponents() is called:
  const std::string port_str = base::NumberToString(https_port_for_testing_);
  DCHECK(https_port_for_testing_ || !use_fake_https_for_testing_);
  if (https_port_for_testing_) {
    // We'll only get here in tests. Tests should always have a non-default
    // port on the input text.
    DCHECK(!http_url.port().empty());
    replacements.SetPortStr(port_str);

    // Change the URL to help with debugging.
    if (use_fake_https_for_testing_)
      replacements.SetRefStr("fake-https");
  }
  if (!use_fake_https_for_testing_) {
    replacements.SetSchemeStr(url::kHttpsScheme);
  }
  return http_url.ReplaceComponents(replacements);
}

GURL HttpsUpgradeService::GetHttpUrl(const GURL& url) const {
  if (use_fake_https_for_testing_) {
    DCHECK_EQ(url::kHttpScheme, url.scheme());
  } else {
    DCHECK_EQ(url::kHttpsScheme, url.scheme());
  }

  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);

  // This needs to be in scope when ReplaceComponents() is called:
  const std::string port_str = base::NumberToString(http_port_for_testing_);
  if (http_port_for_testing_) {
    // We'll only get here in tests. Tests should always have a non-default
    // port on the input text.
    DCHECK(!url.port().empty());
    replacements.SetPortStr(port_str);
  }
  return url.ReplaceComponents(replacements);
}

base::TimeDelta HttpsUpgradeService::GetFallbackDelay() const {
  return fallback_delay_;
}

void HttpsUpgradeService::SetFallbackDelayForTesting(base::TimeDelta delay) {
  fallback_delay_ = delay;
}
