// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/model/browser_about_rewriter.h"

#include <array>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "components/url_formatter/url_fixer.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "url/url_constants.h"

namespace {

struct HostReplacement {
  std::string_view old_host_name;
  std::string_view new_host_name;
};

constexpr std::array<HostReplacement, 2> kHostReplacements = {
    HostReplacement{
        .old_host_name = "about",
        .new_host_name = kChromeUIChromeURLsHost,
    },
    HostReplacement{
        .old_host_name = "sync",
        .new_host_name = kChromeUISyncInternalsHost,
    },
};

}  // namespace

bool WillHandleWebBrowserAboutURL(GURL* url, web::BrowserState* browser_state) {
  GURL original_url = *url;

  // Ensure that any cleanup done by FixupURL happens before the rewriting
  // phase that determines the virtual URL, by including it in an initial
  // URLHandler.  This prevents minor changes from producing a virtual URL,
  // which could lead to a URL spoof.
  *url = url_formatter::FixupURL(url->possibly_invalid_spec(), std::string());

  // Check that about: URLs are fixed up to chrome: by url_formatter::FixupURL.
  // 'about:blank' and 'about:srcdoc' are special-cased in various places in the
  // code so they shouldn't be transformed.
  // (The condition below is that either the URL does not begin with 'about', or
  // if it does, that the path is either 'blank' or 'srcdoc').
  DCHECK(!url->SchemeIs(url::kAboutScheme) ||
         (url->path() == url::kAboutBlankPath ||
          url->path() == url::kAboutSrcdocPath));

  // url_formatter::FixupURL translates about:foo into chrome://foo/.
  if (!url->SchemeIs(kChromeUIScheme)) {
    return false;
  }

  // Translate chrome://newtab back into about://newtab/ so the WebState shows a
  // blank page under the NTP.
  if (url->DeprecatedGetOriginAsURL() == kChromeUINewTabURL) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kAboutScheme);
    *url = url->ReplaceComponents(replacements);
    return *url != original_url;
  }

  std::string new_host(url->host());
  for (const auto& replacement : kHostReplacements) {
    if (new_host != replacement.old_host_name) {
      continue;
    }

    new_host.assign(replacement.new_host_name);
    break;
  }

  GURL::Replacements replacements;
  replacements.SetHostStr(new_host);
  *url = url->ReplaceComponents(replacements);

  // Having re-written the URL, make the chrome: handler process it.
  return false;
}
