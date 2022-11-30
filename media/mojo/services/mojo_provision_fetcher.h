// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_PROVISION_FETCHER_H_
#define MEDIA_MOJO_SERVICES_MOJO_PROVISION_FETCHER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "media/base/provision_fetcher.h"
#include "media/mojo/mojom/provision_fetcher.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// A ProvisionFetcher that proxies to a Remote<mojom::ProvisionFetcher>.
class MEDIA_MOJO_EXPORT MojoProvisionFetcher final : public ProvisionFetcher {
 public:
  explicit MojoProvisionFetcher(
      mojo::PendingRemote<mojom::ProvisionFetcher> provision_fetcher);

  MojoProvisionFetcher(const MojoProvisionFetcher&) = delete;
  MojoProvisionFetcher& operator=(const MojoProvisionFetcher&) = delete;

  ~MojoProvisionFetcher() final;

  // ProvisionFetcher implementation:
  void Retrieve(const GURL& default_url,
                const std::string& request_data,
                ResponseCB response_cb) final;

 private:
  // Callback for mojo::Remote<mojom::ProvisionFetcher>::Retrieve().
  void OnResponse(ResponseCB response_cb,
                  bool success,
                  const std::string& response);

  mojo::Remote<mojom::ProvisionFetcher> provision_fetcher_;

  base::WeakPtrFactory<MojoProvisionFetcher> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_PROVISION_FETCHER_H_
