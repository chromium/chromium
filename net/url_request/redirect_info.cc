// Copyright 2014 The Chromium Authors. All rights reserved.
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
URLRequest::ReferrerPolicy ProcessReferrerPolicyHeaderOnRedirect(
    URLRequest::ReferrerPolicy original_referrer_policy,
    const HttpResponseHeaders* headers) {
  URLRequest::ReferrerPolicy new_policy = original_referrer_policy;
  std::string referrer_policy_header;
  if (headers)
    headers->GetNormalizedHeader("Referrer-Policy", &referrer_policy_header);
  std::vector<std::string> policy_tokens =
      base::SplitString(referrer_policy_header, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  UMA_HISTOGRAM_BOOLEAN("Net.URLRequest.ReferrerPolicyHeaderPresentOnRedirect",
                        !policy_tokens.empty());

  // Per https://w3c.github.io/webappsec-referrer-policy/#unknown-policy-values,
  // use the last recognized policy value, and ignore unknown policies.
  for (const auto& token : policy_tokens) {
    if (base::CompareCaseInsensitiveASCII(token, "no-referrer") == 0) {
      new_policy = URLRequest::NO_REFERRER;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token,
                                          "no-referrer-when-downgrade") == 0) {
      new_policy =
          URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "origin") == 0) {
      new_policy = URLRequest::ORIGIN;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "origin-when-cross-origin") ==
        0) {
      new_policy = URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "unsafe-url") == 0) {
      new_policy = URLRequest::NEVER_CLEAR_REFERRER;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "same-origin") == 0) {
      new_policy = URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(token, "strict-origin") == 0) {
      new_policy =
          URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      continue;
    }

    if (base::CompareCaseInsensitiveASCII(
            token, "strict-origin-when-cross-origin") == 0) {
      new_policy =
          URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      continue;
    }
  }
  return new_policy;
}

}  // namespace

RedirectInfo::RedirectInfo()
    : status_code(-1),
      insecure_scheme_was_upgraded(false),
      new_referrer_policy(
          URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE) {}

RedirectInfo::RedirectInfo(const RedirectInfo& other) = default;

RedirectInfo::~RedirectInfo() = default;

RedirectInfo RedirectInfo::ComputeRedirectInfo(
    const std::string& original_method,
    const GURL& original_url,
    const GURL& original_site_for_cookies,
    URLRequest::FirstPartyURLPolicy original_first_party_url_policy,
    URLRequest::ReferrerPolicy original_referrer_policy,
    const std::string& original_referrer,
    const HttpResponseHeaders* response_headers,
    int http_status_code,
    const GURL& new_location,
    bool insecure_scheme_was_upgraded,
    bool copy_fragment) {
  DCHECK(!response_headers ||
         response_headers->response_code() == http_status_code);

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
    replacements.SetRef(original_url.spec().data(),
                        original_url.parsed_for_possibly_invalid_spec().ref);
    redirect_info.new_url = new_location.ReplaceComponents(replacements);
  } else {
    redirect_info.new_url = new_location;
  }

  redirect_info.insecure_scheme_was_upgraded = insecure_scheme_was_upgraded;

  // Update the first-party URL if appropriate.
  if (original_first_party_url_policy ==
      URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT) {
    redirect_info.new_site_for_cookies = redirect_info.new_url;
  } else {
    redirect_info.new_site_for_cookies = original_site_for_cookies;
  }

  redirect_info.new_referrer_policy = ProcessReferrerPolicyHeaderOnRedirect(
      original_referrer_policy, response_headers);

  // Alter the referrer if redirecting cross-origin (especially HTTP->HTTPS).
  redirect_info.new_referrer =
      URLRequestJob::ComputeReferrerForPolicy(redirect_info.new_referrer_policy,
                                              GURL(original_referrer),
                                              redirect_info.new_url)
          .spec();

  return redirect_info;
}

}  // namespace net
