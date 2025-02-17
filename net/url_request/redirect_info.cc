// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/redirect_info.h"

#include <string_view>

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/url_request/url_request_job.h"

namespace net {

namespace {

std::string ComputeMethodForRedirect(const std::string& method,
                                     int http_status_code) {
  // For 303 redirects, all request methods except HEAD are converted to GET,
  // as per the latest httpbis draft.  The draft also allows POST requests to
  // be converted to GETs when following 301/302 redirects, for historical
  // reasons. Most major browsers do this and so shall we.  Both RFC 2616 and
  // the httpbis draft say to prompt the user to confirm the generation of new
  // requests, other than GET and HEAD requests, but IE omits these prompts and
  // so shall we.
  // See: https://tools.ietf.org/html/rfc7231#section-6.4
  if ((http_status_code == 303 && method != "HEAD") ||
      ((http_status_code == 301 || http_status_code == 302) &&
       method == "POST")) {
    return "GET";
  }
  return method;
}

// A redirect response can contain a Referrer-Policy header
// (https://w3c.github.io/webappsec-referrer-policy/). This function checks for
// a Referrer-Policy header, and parses it if present. Returns the referrer
// policy that should be used for the request.
ReferrerPolicy ProcessReferrerPolicyHeaderOnRedirect(
    ReferrerPolicy original_referrer_policy,
    const std::optional<std::string>& referrer_policy_header) {
  if (referrer_policy_header) {
    return ReferrerPolicyFromHeader(referrer_policy_header.value())
        .value_or(original_referrer_policy);
  }

  return original_referrer_policy;
}

}  // namespace

RedirectInfo::RedirectInfo() = default;

RedirectInfo::RedirectInfo(const RedirectInfo& other) = default;

RedirectInfo::~RedirectInfo() = default;

RedirectInfo RedirectInfo::ComputeRedirectInfo(
    const std::string& original_method,
    const GURL& original_url,
    const SiteForCookies& original_site_for_cookies,
    RedirectInfo::FirstPartyURLPolicy original_first_party_url_policy,
    ReferrerPolicy original_referrer_policy,
    const std::string& original_referrer,
    int http_status_code,
    const GURL& new_location,
    const std::optional<std::string>& referrer_policy_header,
    bool insecure_scheme_was_upgraded,
    bool copy_fragment,
    bool is_signed_exchange_fallback_redirect) {
  RedirectInfo redirect_info;

  redirect_info.status_code = http_status_code;

  // The request method may change, depending on the status code.
  redirect_info.new_method =
      ComputeMethodForRedirect(original_method, http_status_code);

  // Move the reference fragment of the old location to the new one if the
  // new one has none. This duplicates mozilla's behavior.
  if (original_url.is_valid() && original_url.has_ref() &&
      !new_location.has_ref() && copy_fragment) {
    GURL::Replacements replacements;
    // Reference the |ref| directly out of the original URL to avoid a
    // malloc.
    replacements.SetRefStr(original_url.ref_piece());
    redirect_info.new_url = new_location.ReplaceComponents(replacements);
  } else {
    redirect_info.new_url = new_location;
  }

  redirect_info.insecure_scheme_was_upgraded = insecure_scheme_was_upgraded;
  redirect_info.is_signed_exchange_fallback_redirect =
      is_signed_exchange_fallback_redirect;

  // Update the first-party URL if appropriate.
  if (original_first_party_url_policy ==
      FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT) {
    redirect_info.new_site_for_cookies =
        SiteForCookies::FromUrl(redirect_info.new_url);
  } else {
    redirect_info.new_site_for_cookies = original_site_for_cookies;
  }

  redirect_info.new_referrer_policy = ProcessReferrerPolicyHeaderOnRedirect(
      original_referrer_policy, referrer_policy_header);

  // Alter the referrer if redirecting cross-origin (especially HTTP->HTTPS).
  redirect_info.new_referrer =
      URLRequestJob::ComputeReferrerForPolicy(redirect_info.new_referrer_policy,
                                              GURL(original_referrer),
                                              redirect_info.new_url)
          .spec();

  return redirect_info;
}

}  // namespace net
