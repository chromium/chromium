// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/web_app_url_config.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace webapps {

namespace {

base::flat_set<std::string>& ValidChromeUrlHostsForTesting() {
  static base::NoDestructor<base::flat_set<std::string>> hosts;
  return *hosts.get();
}

}  // namespace

bool IsUrlEligibleForWebApp(const GURL& url) {
  if (!url.is_valid() || url.inner_url()) {
    return false;
  }

  if (url.IsAboutBlank()) {
    return false;
  }

  bool allow_extension_apps = true;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Stop allowing apps to be extension URLs when the shortcuts are separated -
  // they can be extension URLs instead.
  allow_extension_apps = false;
#endif

  // TODO(crbug.com/40793595): Remove chrome-extension scheme.
  if (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme) ||
      (allow_extension_apps && url.SchemeIs("chrome-extension"))) {
    return true;
  }

  // chrome://password-manager is eligible, as are test-registered hosts.
  if (url.SchemeIs(content::kChromeUIScheme) &&
      (url.GetHost() == password_manager::kChromeUIPasswordManagerHost ||
       ValidChromeUrlHostsForTesting().contains(url.GetHost()))) {
    return true;
  }

  return false;
}

base::ScopedClosureRunner AddValidChromeUrlHostForTesting(  // IN-TEST
    const std::string& host) {
  CHECK(ValidChromeUrlHostsForTesting().insert(host).second);  // IN-TEST
  return base::ScopedClosureRunner(base::BindOnce(
      [](const std::string& host) {
        CHECK(ValidChromeUrlHostsForTesting().contains(host));  // IN-TEST
        ValidChromeUrlHostsForTesting().erase(host);            // IN-TEST
      },
      host));
}

}  // namespace webapps
