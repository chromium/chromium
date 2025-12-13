// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_ORIGIN_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_ORIGIN_H_

#include <string>

#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

// Represents the origin of an Isolated Web App, which is derived from its
// Signed Web Bundle ID. This class is the browser-agnostic representation of an
// IWA's identity.
class IwaOrigin {
 public:
  struct StoragePartitionConfigOptions {
    std::string partition_name;
    bool in_memory;
  };

  explicit IwaOrigin(const web_package::SignedWebBundleId& web_bundle_id);

  // Creates an `IwaOrigin` instance from the given URL, or an error
  // message if the URL isn't a valid `isolated-app://` URL.
  static base::expected<IwaOrigin, std::string> Create(const GURL& url);

  const url::Origin& origin() const { return origin_; }

  const web_package::SignedWebBundleId& web_bundle_id() const {
    return web_bundle_id_;
  }

  content::StoragePartitionConfig storage_partition_config(
      content::BrowserContext* browser_context,
      std::optional<StoragePartitionConfigOptions> = std::nullopt) const;

  bool operator<=>(const IwaOrigin&) const = default;

 private:
  url::Origin origin_;
  web_package::SignedWebBundleId web_bundle_id_;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_ORIGIN_H_
