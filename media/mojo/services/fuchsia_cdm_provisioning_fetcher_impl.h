// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_FUCHSIA_CDM_PROVISIONING_FETCHER_IMPL_H_
#define MEDIA_MOJO_SERVICES_FUCHSIA_CDM_PROVISIONING_FETCHER_IMPL_H_

#include <fuchsia/media/drm/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <memory>

#include "base/functional/callback_forward.h"
#include "media/base/provision_fetcher.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

// Server end implementation of the fuchsia.media.drm.ProvisioningFetcher
// protocol. The client end is provided to the fuchsia.media.drm.KeySystem so
// that the KeySystem can invoke provisioning retrieval when necessary.
class MEDIA_MOJO_EXPORT FuchsiaCdmProvisioningFetcherImpl
    : public fuchsia::media::drm::ProvisioningFetcher {
 public:
  explicit FuchsiaCdmProvisioningFetcherImpl(
      CreateFetcherCB create_fetcher_callback);
  ~FuchsiaCdmProvisioningFetcherImpl() override;

  // Disallow copy and move
  FuchsiaCdmProvisioningFetcherImpl(const FuchsiaCdmProvisioningFetcherImpl&) =
      delete;
  FuchsiaCdmProvisioningFetcherImpl(FuchsiaCdmProvisioningFetcherImpl&&) =
      delete;
  FuchsiaCdmProvisioningFetcherImpl& operator=(
      const FuchsiaCdmProvisioningFetcherImpl&) = delete;
  FuchsiaCdmProvisioningFetcherImpl& operator=(
      FuchsiaCdmProvisioningFetcherImpl&&) = delete;

  fidl::InterfaceHandle<fuchsia::media::drm::ProvisioningFetcher> Bind(
      base::OnceClosure error_callback);

  // fuchsia::media::drm::ProvisioningFetcher implementation.
  void Fetch(fuchsia::media::drm::ProvisioningRequest request,
             FetchCallback callback) override;

 protected:
  void OnRetrieveComplete(FetchCallback callback,
                          bool success,
                          const std::string& response);
  void OnError(zx_status_t status);

 private:
  CreateFetcherCB create_fetcher_callback_;
  fidl::Binding<fuchsia::media::drm::ProvisioningFetcher> binding_;
  base::OnceClosure error_callback_;
  std::unique_ptr<media::ProvisionFetcher> fetcher_;
  bool retrieve_in_progress_ = false;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_FUCHSIA_CDM_PROVISIONING_FETCHER_IMPL_H_
