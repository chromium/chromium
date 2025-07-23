// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_CLIENT_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_CLIENT_H_

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/version.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace web_app {

// This singleton acts as a bridge between the browser-independent IWA layer and
// the embedder layer (i.e. Chrome).
class IwaClient {
 public:
  IwaClient(const IwaClient&) = delete;
  IwaClient& operator=(const IwaClient&) = delete;

  static IwaClient* GetInstance();

  // Tells whether the IWA identifier by `web_bundle_id` comes from a trusted
  // source and can thus be used/installed according to the embedder-defined
  // rules.
  virtual base::expected<void, std::string> ValidateTrust(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id,
      bool dev_mode) = 0;

  // Infers the web bundle id of the IWA handling a particular URL with respect
  // to the embedder-defined format.
  // TODO(crbug.com/431980377): Consider moving chrome::kIsolatedAppScheme to
  // components/webapps/isolated_web_apps/ to remove this link.
  virtual base::expected<web_package::SignedWebBundleId, std::string>
  CreateWebBundleIdFromURL(const GURL& url) = 0;

  // Infers the base URL for a signed web bundle with this `web_bundle_id`;
  // Resources from this web bundle will be served relative to it.
  // TODO(crbug.com/431980377): Consider moving chrome::kIsolatedAppScheme to
  // components/webapps/isolated_web_apps/ to remove this link.
  virtual GURL CreateBaseURLForWebBundleId(
      const web_package::SignedWebBundleId& web_bundle_id) = 0;

  // Tells the embedder (who manages the app system) to run the supplied
  // `callback` once all windows of the app defined by `web_bundle_id` are
  // closed.
  virtual void RunWhenAppCloses(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id,
      base::OnceClosure callback) = 0;

  // TODO
  virtual content::StoragePartition* GetStoragePartition(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id) = 0;

 protected:
  IwaClient();
  virtual ~IwaClient();
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_CLIENT_H_
