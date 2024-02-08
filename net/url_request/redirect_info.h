// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_REDIRECT_INFO_H_
#define NET_URL_REQUEST_REDIRECT_INFO_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/site_for_cookies.h"
#include "net/url_request/referrer_policy.h"
#include "url/gurl.h"

namespace net {

// RedirectInfo captures information about a redirect and any fields in a
// request that change.
struct NET_EXPORT RedirectInfo {
  // First-party URL redirect policy: During server redirects, the first-party
  // URL for cookies normally doesn't change. However, if the request is a
  // top-level first-party request, the first-party URL should be updated to the
  // URL on every redirect.
  enum class FirstPartyURLPolicy {
    NEVER_CHANGE_URL,
    UPDATE_URL_ON_REDIRECT,
  };

  RedirectInfo();
  RedirectInfo(const RedirectInfo& other);
  ~RedirectInfo();

  // Computes a new RedirectInfo.
  static RedirectInfo ComputeRedirectInfo(
      // The following arguments |original_*| are the properties of the original
      // request.
      const std::string& original_method,
      const GURL& original_url,
      const SiteForCookies& original_site_for_cookies,
      FirstPartyURLPolicy original_first_party_url_policy,
      ReferrerPolicy original_referrer_policy,
      const std::string& original_referrer,
      // The HTTP status code of the redirect response.
      int http_status_code,
      // The new location URL of the redirect response.
      const GURL& new_location,
      // Referrer-Policy header of the redirect response.
      const std::optional<std::string>& referrer_policy_header,
      // Whether the URL was upgraded to HTTPS due to upgrade-insecure-requests.
      bool insecure_scheme_was_upgraded,
      // This method copies the URL fragment of the original URL to the new URL
      // by default. Set false only when the network delegate has set the
      // desired redirect URL (with or without fragment), so it must not be
      // changed any more.
      bool copy_fragment = true,
      // Whether the redirect is caused by a failure of signed exchange loading.
      bool is_signed_exchange_fallback_redirect = false);

  // The status code for the redirect response. This is almost redundant with
  // the response headers, but some URLRequestJobs emit redirects without
  // headers.
  int status_code = -1;

  // The new request method. Depending on the response code, the request method
  // may change.
  std::string new_method;

  // The new request URL.
  GURL new_url;

  // The new first-party for cookies.
  SiteForCookies new_site_for_cookies;

  // The new HTTP referrer header.
  std::string new_referrer;

  // True if this redirect was upgraded to HTTPS due to the
  // upgrade-insecure-requests policy.
  bool insecure_scheme_was_upgraded = false;

  // True if this is a redirect from Signed Exchange to its fallback URL.
  bool is_signed_exchange_fallback_redirect = false;

  // The new referrer policy that should be obeyed if there are
  // subsequent redirects.
  ReferrerPolicy new_referrer_policy =
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;

  // When navigation is restarted due to a Critical-CH header this stores the
  // time at which the the restart was initiated.
  base::TimeTicks critical_ch_restart_time;
};

}  // namespace net

#endif  // NET_URL_REQUEST_REDIRECT_INFO_H_
