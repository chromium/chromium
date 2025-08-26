// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/types/iwa_origin.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/scheme.h"

namespace web_app {

IwaOrigin::IwaOrigin(const web_package::SignedWebBundleId& web_bundle_id)
    : origin_(
          url::Origin::CreateFromNormalizedTuple(webapps::kIsolatedAppScheme,
                                                 web_bundle_id.id(),
                                                 /*port=*/0)),
      web_bundle_id_(web_bundle_id) {}

// static
base::expected<IwaOrigin, std::string> IwaOrigin::Create(const GURL& url) {
  if (!url.is_valid()) {
    return base::unexpected("Invalid URL");
  }
  if (!url.SchemeIs(webapps::kIsolatedAppScheme)) {
    return base::unexpected(
        base::StrCat({"The URL scheme must be ", webapps::kIsolatedAppScheme,
                      ", but was ", url.scheme()}));
  }

  // Valid isolated-app:// `GURL`s can never include credentials or ports, since
  // the scheme is configured as `url::SCHEME_WITH_HOST`. The `DCHECK` is here
  // just in case, but should never trigger as long as the scheme is configured
  // correctly.
  DCHECK(!url.has_username() && !url.has_password() && !url.has_port() &&
         url.IsStandard());

  return web_package::SignedWebBundleId::Create(url.host())
      .transform([](const web_package::SignedWebBundleId& web_bundle_id) {
        return IwaOrigin(web_bundle_id);
      })
      .transform_error([&](const std::string& error) {
        return base::StrCat(
            {"The host of isolated-app:// URLs must be a valid Signed "
             "Web Bundle ID (got ",
             url.host(), "): ", error});
      });
}

}  // namespace web_app
