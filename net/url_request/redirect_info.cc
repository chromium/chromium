// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/redirect_info.h"

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
  ReferrerPolicy new_policy = original_referrer_policy;
  std::vector<base::StringPiece> policy_tokens;
  if (referrer_policy_header) {
    policy_tokens = base::SplitStringPiece(*referrer_policy_header, ",",
                                           base::TRIM_WHITESPACE,
                                           base::SPLIT_WANT_NONEMPTY);
  }

  // Per https://w3c.github.io/webappsec-referrer-policy/#unknown-policy-values,
  // use the last recognized policy value, and ignore unknown policies.
  for (const auto& token : policy_tokens) {
    if (base::CompareCaseInsensitiveASCII(token, "no-referrer") == 0) {
      new_policy = ReferrerPolicy::NO_REFERRER;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token,
                                          "no-referrer-when-downgrade") == 0) {
      new_policy = ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "origin") == 0) {
      new_policy = ReferrerPolicy::ORIGIN;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "origin-when-cross-origin") ==
        0) {
      new_policy = ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "unsafe-url") == 0) {
      new_policy = ReferrerPolicy::NEVER_CLEAR;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "same-origin") == 0) {
      new_policy = ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "strict-origin") == 0) {
      new_policy =
          ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(
            token, "strict-origin-when-cross-origin") == 0) {
      new_policy =
          ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      continue;
    }
  }
  return new_policy;
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
