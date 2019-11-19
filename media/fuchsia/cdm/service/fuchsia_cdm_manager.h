// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_SERVICE_FUCHSIA_CDM_MANAGER_H_
#define MEDIA_FUCHSIA_CDM_SERVICE_FUCHSIA_CDM_MANAGER_H_

#include <fuchsia/media/drm/cpp/fidl.h>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/provision_fetcher.h"

namespace url {
class Origin;
}  // namespace url

namespace media {

// Create and connect to Fuchsia CDM service. It will provision the origin if
// needed. When provision is needed by multiple web pages for the same origin,
// it will chain all the concurrent provision requests and make sure we
// only handle one provision request for the origin at a time. This is mainly
// because the latest provision response will invalidate old provisioned cert,
// as well as the license sessions. We want to make sure once the channel to
// CDM service is established, nothing from Chromium breaks it.
class FuchsiaCdmManager {
 public:
  // Handler for key system specific logic.
  class KeySystemHandler {
   public:
    virtual ~KeySystemHandler() = default;

    // Create CDM for license management and decryption.
    virtual void CreateCdm(
        fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
            request) = 0;

    // Create Provisioner for origin provision. Impl may return nullptr if
    // Provisioner is not supported, in which case the call should assume the
    // origin is already provisioned.
    virtual fuchsia::media::drm::ProvisionerPtr CreateProvisioner() = 0;
  };

  // A map from key system to its KeySystemHandler.
  using KeySystemHandlerMap =
      base::flat_map<std::string, std::unique_ptr<KeySystemHandler>>;

  explicit FuchsiaCdmManager(KeySystemHandlerMap handlers);
  ~FuchsiaCdmManager();

  void CreateAndProvision(
      const std::string& key_system,
      const url::Origin& origin,
      CreateFetcherCB create_fetcher_cb,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request);

 private:
  class OriginProvisioner;

  OriginProvisioner* GetProvisioner(const std::string& key_system,
                                    const url::Origin& origin,
                                    KeySystemHandler* handler);

  void OnProvisionResult(
      KeySystemHandler* handler,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request,
      bool success);

  const KeySystemHandlerMap handlers_;

  // key system -> OriginProvisioner
  base::flat_map<std::string, std::unique_ptr<OriginProvisioner>>
      origin_provisioners_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(FuchsiaCdmManager);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_SERVICE_FUCHSIA_CDM_MANAGER_H_
