// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_util.h"

#import "base/i18n/rtl.h"
#import "components/url_formatter/elide_url.h"
#import "components/url_formatter/url_formatter.h"
#import "url/gurl.h"

namespace {

// Strip a trivial "www." subdomain from `url`'s host.
// `url_formatter::StripWWW` applies the same eligibility rules as
// `url_formatter::kFormatUrlOmitTrivialSubdomains` (e.g. "www." is kept when it
// is part of the registrable domain or for intranet hosts).
GURL StripTrivialSubdomain(const GURL& url) {
  // Invalid URLs are returned unchanged because `GURL::ReplaceComponents`
  // would turn them into an empty `GURL`.
  if (!url.is_valid() || !url.has_host()) {
    return url;
  }
  const std::string host_without_www =
      url_formatter::StripWWW(std::string(url.host()));
  if (host_without_www == url.host()) {
    return url;
  }
  GURL::Replacements replacements;
  replacements.SetHostStr(host_without_www);
  return url.ReplaceComponents(replacements);
}

}  // namespace

std::u16string FormatUrlForBottomSheetDisplay(const GURL& url) {
  // `FormatUrlForSecurityDisplay` cannot elide subdomains, so the trivial
  // subdomain is stripped from the canonical (punycode) host beforehand.
  std::u16string formatted_url = url_formatter::FormatUrlForSecurityDisplay(
      StripTrivialSubdomain(url),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::i18n::WrapStringWithLTRFormatting(&formatted_url);
  return formatted_url;
}
