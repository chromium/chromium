// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/model/browser_about_rewriter.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "components/url_formatter/url_fixer.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "url/url_constants.h"

namespace {

const struct HostReplacement {
  const char* old_host_name;
  const char* new_host_name;
} kHostReplacements[] = {
    {"about", kChromeUIChromeURLsHost},
    {"sync", kChromeUISyncInternalsHost},
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
  // 'about:blank' is special-cased in various places in the code so it
  // shouldn't be transformed.
  DCHECK(!url->SchemeIs(url::kAboutScheme) ||
         (url->path() == url::kAboutBlankPath));

  // url_formatter::FixupURL translates about:foo into chrome://foo/.
  if (!url->SchemeIs(kChromeUIScheme))
    return false;

  // Translate chrome://newtab back into about://newtab/ so the WebState shows a
  // blank page under the NTP.
  if (url->DeprecatedGetOriginAsURL() == kChromeUINewTabURL) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kAboutScheme);
    *url = url->ReplaceComponents(replacements);
    return *url != original_url;
  }

  std::string host(url->host());
  for (size_t i = 0; i < std::size(kHostReplacements); ++i) {
    if (host != kHostReplacements[i].old_host_name)
      continue;

    host.assign(kHostReplacements[i].new_host_name);
    break;
  }

  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  *url = url->ReplaceComponents(replacements);

  // Having re-written the URL, make the chrome: handler process it.
  return false;
}
