// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/redirect_info.h"

#include "base/strings/string_number_conversions.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

TEST(RedirectInfoTest, MethodForRedirect) {
  struct TestCase {
    const char* original_method;
    int http_status_code;
    const char* expected_new_method;
  };
  const TestCase kTests[] = {
      {"GET", 301, "GET"},   {"GET", 302, "GET"},   {"GET", 303, "GET"},
      {"GET", 307, "GET"},   {"GET", 308, "GET"},   {"HEAD", 301, "HEAD"},
      {"HEAD", 302, "HEAD"}, {"HEAD", 303, "HEAD"}, {"HEAD", 307, "HEAD"},
      {"HEAD", 308, "HEAD"}, {"POST", 301, "GET"},  {"POST", 302, "GET"},
      {"POST", 303, "GET"},  {"POST", 307, "POST"}, {"POST", 308, "POST"},
      {"PUT", 301, "PUT"},   {"PUT", 302, "PUT"},   {"PUT", 303, "GET"},
      {"PUT", 307, "PUT"},   {"PUT", 308, "PUT"},
  };

  const GURL kOriginalUrl = GURL("https://foo.test/original");
  const GURL kOriginalSiteForCookies = GURL("https://foo.test/");
  const url::Origin kOriginalTopFrameOrigin = url::Origin::Create(kOriginalUrl);
  const URLRequest::FirstPartyURLPolicy kOriginalFirstPartyUrlPolicy =
      net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  const URLRequest::ReferrerPolicy kOriginalReferrerPolicy =
      net::URLRequest::NEVER_CLEAR_REFERRER;
  const std::string kOriginalReferrer = "";
  const GURL kNewLocation = GURL("https://foo.test/redirected");
  const bool kInsecureSchemeWasUpgraded = false;
  const bool kCopyFragment = true;

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message()
                 << "original_method: " << test.original_method
                 << " http_status_code: " << test.http_status_code);

    RedirectInfo redirect_info = RedirectInfo::ComputeRedirectInfo(
        test.original_method, kOriginalUrl,
        url::Origin::Create(kOriginalSiteForCookies), kOriginalSiteForCookies,
        kOriginalFirstPartyUrlPolicy, kOriginalReferrerPolicy,
        kOriginalReferrer, test.http_status_code, kNewLocation,
        base::nullopt /* referrer_policy_header */, kInsecureSchemeWasUpgraded,
        kCopyFragment);

    EXPECT_EQ(test.expected_new_method, redirect_info.new_method);
    EXPECT_EQ(test.http_status_code, redirect_info.status_code);
    EXPECT_EQ(kNewLocation, redirect_info.new_url);
  }
}

TEST(RedirectInfoTest, CopyFragment) {
  struct TestCase {
    bool copy_fragment;
    const char* original_url;
    const char* new_location;
    const char* expected_new_url;
  };
  const TestCase kTests[] = {
      {true, "http://foo.test/original", "http://foo.test/redirected",
       "http://foo.test/redirected"},
      {true, "http://foo.test/original#1", "http://foo.test/redirected",
       "http://foo.test/redirected#1"},
      {true, "http://foo.test/original#1", "http://foo.test/redirected#2",
       "http://foo.test/redirected#2"},
      {false, "http://foo.test/original", "http://foo.test/redirected",
       "http://foo.test/redirected"},
      {false, "http://foo.test/original#1", "http://foo.test/redirected",
       "http://foo.test/redirected"},
      {false, "http://foo.test/original#1", "http://foo.test/redirected#2",
       "http://foo.test/redirected#2"},
  };

  const std::string kOriginalMethod = "GET";
  const GURL kOriginalSiteForCookies = GURL("https://foo.test/");
  const URLRequest::FirstPartyURLPolicy kOriginalFirstPartyUrlPolicy =
      net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  const URLRequest::ReferrerPolicy kOriginalReferrerPolicy =
      net::URLRequest::NEVER_CLEAR_REFERRER;
  const std::string kOriginalReferrer = "";
  const int kHttpStatusCode = 301;
  const bool kInsecureSchemeWasUpgraded = false;

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message()
                 << "copy_fragment: " << test.copy_fragment
                 << " original_url: " << test.original_url
                 << " new_location: " << test.new_location);

    RedirectInfo redirect_info = RedirectInfo::ComputeRedirectInfo(
        kOriginalMethod, GURL(test.original_url),
        url::Origin::Create(kOriginalSiteForCookies), kOriginalSiteForCookies,
        kOriginalFirstPartyUrlPolicy, kOriginalReferrerPolicy,
        kOriginalReferrer, kHttpStatusCode, GURL(test.new_location),
        base::nullopt /* referrer_policy_header */, kInsecureSchemeWasUpgraded,
        test.copy_fragment);

    EXPECT_EQ(GURL(test.expected_new_url), redirect_info.new_url);
  }
}

TEST(RedirectInfoTest, FirstPartyURLPolicy) {
  struct TestCase {
    URLRequest::FirstPartyURLPolicy original_first_party_url_policy;
    const char* expected_new_site_for_cookies;
  };
  const TestCase kTests[] = {
      {URLRequest::NEVER_CHANGE_FIRST_PARTY_URL, "https://foo.test/"},
      {URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT,
       "https://foo.test/redirected"},
  };

  const std::string kOriginalMethod = "GET";
  const GURL kOriginalUrl = GURL("https://foo.test/");
  const GURL kOriginalSiteForCookies = GURL("https://foo.test/");
  const URLRequest::ReferrerPolicy kOriginalReferrerPolicy =
      net::URLRequest::NEVER_CLEAR_REFERRER;
  const std::string kOriginalReferrer = "";
  const GURL kNewLocation = GURL("https://foo.test/redirected");
  const bool kInsecureSchemeWasUpgraded = false;
  const int kHttpStatusCode = 301;
  const bool kCopyFragment = true;

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message()
                 << "original_first_party_url_policy: "
                 << static_cast<int>(test.original_first_party_url_policy));

    RedirectInfo redirect_info = RedirectInfo::ComputeRedirectInfo(
        kOriginalMethod, kOriginalUrl,
        url::Origin::Create(kOriginalSiteForCookies), kOriginalSiteForCookies,
        test.original_first_party_url_policy, kOriginalReferrerPolicy,
        kOriginalReferrer, kHttpStatusCode, kNewLocation,
        base::nullopt /* referrer_policy_header */, kInsecureSchemeWasUpgraded,
        kCopyFragment);

    EXPECT_EQ(GURL(test.expected_new_site_for_cookies),
              redirect_info.new_site_for_cookies);
  }
}

// A helper for making a base::Optional url::Origin given a string.
base::Optional<url::Origin> CreateInitiator(const char* url) {
  return base::make_optional<url::Origin>(url::Origin::Create(GURL(url)));
}

TEST(RedirectInfoTest, ReferrerPolicy) {
  struct TestCase {
    base::Optional<url::Origin> initiator;
    const char* original_url;
    const char* original_referrer;
    const char* response_headers;
    URLRequest::ReferrerPolicy original_referrer_policy;
    URLRequest::ReferrerPolicy expected_new_referrer_policy;
    const char* expected_referrer;
  };

  const TestCase kTests[] = {
      // If a redirect serves 'Referrer-Policy: no-referrer', then the referrer
      // should be cleared.
      {CreateInitiator("http://foo.test"),
       "http://foo.test/one" /* original url */,
       "http://foo.test/one" /* original referrer */,
       "Location: http://foo.test/test\n"
       "Referrer-Policy: no-referrer\n",
       // original policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       URLRequest::NO_REFERRER /* expected new policy */,
       "" /* expected new referrer */},
      // Same as above but for the legacy keyword 'never', which should not be
      // supported.
      {CreateInitiator("http://foot.test"),
       "http://foo.test/one" /* original url */,
       "http://foo.test/one" /* original referrer */,
       "Location: http://foo.test/test\nReferrer-Policy: never\n",
       // original policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       // expected new policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "http://foo.test/one" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: no-referrer-when-downgrade',
      // then the referrer should be cleared on downgrade, even if the original
      // request's policy specified that the referrer should never be cleared.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: http://foo.test\n"
       "Referrer-Policy: no-referrer-when-downgrade\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       // expected new policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "" /* expected new referrer */},

      // Same as above but for the legacy keyword 'default', which should not be
      // supported.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: http://foo.test\n"
       "Referrer-Policy: default\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       // expected new policy
       URLRequest::NEVER_CLEAR_REFERRER,
       "https://foo.test/one" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: no-referrer-when-downgrade',
      // the referrer should not be cleared for a non-downgrading redirect. But
      // the policy should be updated.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: https://foo.test\n"
       "Referrer-Policy: no-referrer-when-downgrade\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       // expected new policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "https://foo.test/one" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: origin', then the referrer
      // should be stripped to its origin, even if the original request's policy
      // specified that the referrer should never be cleared.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: https://foo.test/two\n"
       "Referrer-Policy: origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::ORIGIN /* expected new policy */,
       "https://foo.test/" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: origin-when-cross-origin' and
      // then the referrer should be untouched for a same-origin redirect ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://foo.test/two\n"
       "Referrer-Policy: origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://foo.test/referrer" /* expected new referrer */},

      // ... but should be stripped to the origin for a cross-origin redirect
      // ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://foo.test/" /* expected new referrer */},

      // ... even when the referrer is same-origin with the redirect ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://bar.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://bar.test/" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: origin-when-cross-origin' and
      // the request has no initiator, then the referrer should be untouched for
      // a same-origin redirect ...
      {base::nullopt /* initiator */, "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://foo.test/two\n"
       "Referrer-Policy: origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://foo.test/referrer" /* expected new referrer */},

      // ... but should be stripped to the origin for a cross-origin redirect
      // ...
      // (The referrer string is used to determine same-origin-ness in the
      // absence of an initiator).
      {base::nullopt /* initiator */, "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://foo.test/" /* expected new referrer */},

      // ... except for when the referrer string is same-origin with the
      // redirect.
      {base::nullopt /* initiator */, "https://foo.test/one" /* original url */,
       "https://bar.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://bar.test/referrer" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: same-origin', then the referrer
      // should be untouched for a same-origin redirect,
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://foo.test/two\n"
       "Referrer-Policy: same-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN /* new policy */
       ,
       "https://foo.test/referrer" /* expected new referrer */},

      // ... but should be cleared for a cross-origin redirect ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: same-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
       "" /* expected new referrer */},

      // ... even when the referrer is same-origin with the redirect ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://bar.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: same-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
       "" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: same-origin' and the request has
      // no initiator, then the referrer should be untouched for a same-origin
      // redirect,
      {base::nullopt /* initiator */, "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://foo.test/two\n"
       "Referrer-Policy: same-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN /* new policy */
       ,
       "https://foo.test/referrer" /* expected new referrer */},

      // ... but should be cleared for a cross-origin redirect ...
      {base::nullopt /* initiator */, "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: same-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
       "" /* expected new referrer */},

      // ... except when the referrer is same-origin with the redirect ...
      // (This is because in the abscence of an initiator origin, the referrer
      // string is used for same-origin-ness comparisons.)
      {base::nullopt /* initiator */, "https://foo.test/one" /* original url */,
       "https://bar.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: same-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
       "https://bar.test/referrer" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: strict-origin', then the
      // referrer should be the referrer origin for non-downgrading same-origin
      // redirects, ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://foo.test/two\n"
       "Referrer-Policy: strict-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "https://foo.test/" /* expected new referrer */},
      {CreateInitiator("http://foo.test"),
       "http://foo.test/one" /* original url */,
       "http://foo.test/referrer" /* original referrer */,
       "Location: http://foo.test/two\n"
       "Referrer-Policy: strict-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "http://foo.test/" /* expected new referrer */},

      // ... as well as for non-downgrading cross-origin redirects ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: strict-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "https://foo.test/" /* expected new referrer */},
      {CreateInitiator("http://foo.test"),
       "http://foo.test/one" /* original url */,
       "http://foo.test/referrer" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: strict-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "http://foo.test/" /* expected new referrer */},

      // ... but should be cleared for downgrading redirect, regardless of
      // same-origin-ness.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: http://foo.test/two\n"
       "Referrer-Policy: strict-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "" /* expected new referrer */},
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: strict-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy:
      // strict-origin-when-cross-origin', then the referrer should be preserved
      // for non-downgrading same-origin redirects,
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://foo.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "https://foo.test/referrer" /* expected new referrer */},
      {CreateInitiator("http://foo.test"),
       "http://foo.test/one" /* original url */,
       "http://foo.test/referrer" /* original referrer */,
       "Location: http://foo.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "http://foo.test/referrer" /* expected new referrer */},

      // ... but should be stripped to the origin for non-downgrading
      // cross-origin redirects, ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "https://foo.test/" /* expected new referrer */},
      {CreateInitiator("http://foo.test"),
       "http://foo.test/one" /* original url */,
       "http://foo.test/referrer" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "http://foo.test/" /* expected new referrer */},

      // ... even when the referrer is same-origin with the redirect ...
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://bar.test/referrer" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "https://bar.test/" /* expected new referrer */},
      {CreateInitiator("http://foo.test"),
       "http://foo.test/one" /* original url */,
       "http://bar.test/referrer" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "http://bar.test/" /* expected new referrer */},

      // ... and should be cleared entirely for all downgrading redirects,
      // regardless of same-origin-ness ...
      // (These test that the downgrading characteristic of the redirect takes
      // precedent over the same-origin-ness).
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: http://foo.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "" /* expected new referrer */},
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/referrer" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: strict-origin-when-cross-origin\n",
       URLRequest::NEVER_CLEAR_REFERRER /* original policy */,
       URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
       "" /* expected new referrer */},

      // If a redirect serves 'Referrer-Policy: unsafe-url', then the referrer
      // should remain, even if originally set to clear on downgrade.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: unsafe-url\n",
       URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* original policy */,
       URLRequest::NEVER_CLEAR_REFERRER /* expected new policy */,
       "https://foo.test/one" /* expected new referrer */},

      // Same as above but for the legacy keyword 'always', which should not be
      // supported.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: always\n",
       URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://foo.test/" /* expected new referrer */},

      // An invalid keyword should leave the policy untouched.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: not-a-valid-policy\n",
       URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://foo.test/" /* expected new referrer */},

      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: http://bar.test/two\n"
       "Referrer-Policy: not-a-valid-policy\n",
       // original policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       // expected new policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "" /* expected new referrer */},

      // The last valid keyword should take precedence.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: unsafe-url\n"
       "Referrer-Policy: not-a-valid-policy\n",
       URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* original policy */,
       URLRequest::NEVER_CLEAR_REFERRER /* expected new policy */,
       "https://foo.test/one" /* expected new referrer */},

      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: unsafe-url\n"
       "Referrer-Policy: origin\n",
       URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* original policy */,
       URLRequest::ORIGIN /* expected new policy */,
       "https://foo.test/" /* expected new referrer */},
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://bar.test/one" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: unsafe-url\n"
       "Referrer-Policy: origin\n",
       URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* original policy */,
       URLRequest::ORIGIN /* expected new policy */,
       "https://bar.test/" /* expected new referrer */},

      // An empty header should not affect the request.
      {CreateInitiator("https://foo.test"),
       "https://foo.test/one" /* original url */,
       "https://foo.test/one" /* original referrer */,
       "Location: https://bar.test/two\n"
       "Referrer-Policy: \n",
       URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* original policy */,
       URLRequest::
           ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN /* expected new policy */,
       "https://foo.test/" /* expected new referrer */},

      // A redirect response without Referrer-Policy header should not affect
      // the policy and the referrer.
      {CreateInitiator("http://foo.test"),
       "http://foo.test/one" /* original url */,
       "http://foo.test/one" /* original referrer */,
       "Location: http://foo.test/test\n",
       // original policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       // expected new policy
       URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "http://foo.test/one" /* expected new referrer */},
  };

  const std::string kOriginalMethod = "GET";
  const GURL kOriginalSiteForCookies = GURL("https://foo.test/");
  const URLRequest::FirstPartyURLPolicy kOriginalFirstPartyUrlPolicy =
      net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  const bool kInsecureSchemeWasUpgraded = false;
  const bool kCopyFragment = true;

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message()
                 << "original_url: " << test.original_url
                 << " original_referrer: " << test.original_referrer
                 << " response_headers: " << test.response_headers
                 << " original_referrer_policy: "
                 << static_cast<int>(test.original_referrer_policy));

    std::string response_header_text =
        "HTTP/1.1 302 Redirect\n" + std::string(test.response_headers);
    std::string raw_headers =
        HttpUtil::AssembleRawHeaders(response_header_text);
    auto response_headers =
        base::MakeRefCounted<HttpResponseHeaders>(raw_headers);
    EXPECT_EQ(302, response_headers->response_code());

    std::string location_string;
    EXPECT_TRUE(response_headers->IsRedirect(&location_string));
    const GURL original_url = GURL(test.original_url);
    const GURL new_location = original_url.Resolve(location_string);

    RedirectInfo redirect_info_with_initiator =
        RedirectInfo::ComputeRedirectInfo(
            kOriginalMethod, original_url, test.initiator,
            kOriginalSiteForCookies, kOriginalFirstPartyUrlPolicy,
            test.original_referrer_policy, test.original_referrer,
            response_headers->response_code(), new_location,
            RedirectUtil::GetReferrerPolicyHeader(response_headers.get()),
            kInsecureSchemeWasUpgraded, kCopyFragment);

    EXPECT_EQ(test.expected_new_referrer_policy,
              redirect_info_with_initiator.new_referrer_policy);
    EXPECT_EQ(test.expected_referrer,
              redirect_info_with_initiator.new_referrer);
  }
}

}  // namespace
}  // namespace net
