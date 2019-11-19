// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/cdm/cdm_proxy.h"
#include "media/mojo/mojom/cdm_proxy.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace media {

class MojoCdmServiceContext;

// A mojom::CdmProxy implementation backed by a media::CdmProxy.
class MEDIA_MOJO_EXPORT MojoCdmProxyService : public mojom::CdmProxy,
                                              public CdmProxy::Client {
 public:
  MojoCdmProxyService(std::unique_ptr<::media::CdmProxy> cdm_proxy,
                      MojoCdmServiceContext* context);

  ~MojoCdmProxyService() final;

  // mojom::CdmProxy implementation.
  void Initialize(mojo::PendingAssociatedRemote<mojom::CdmProxyClient> client,
                  InitializeCallback callback) final;
  void Process(media::CdmProxy::Function function,
               uint32_t crypto_session_id,
               const std::vector<uint8_t>& input_data,
               uint32_t expected_output_data_size,
               ProcessCallback callback) final;
  void CreateMediaCryptoSession(
      const std::vector<uint8_t>& input_data,
      CreateMediaCryptoSessionCallback callback) final;
  void SetKey(uint32_t crypto_session_id,
              const std::vector<uint8_t>& key_id,
              media::CdmProxy::KeyType key_type,
              const std::vector<uint8_t>& key_blob,
              SetKeyCallback callback) final;
  void RemoveKey(uint32_t crypto_session_id,
                 const std::vector<uint8_t>& key_id,
                 RemoveKeyCallback callback) final;

  // CdmProxy::Client implementation.
  void NotifyHardwareReset() final;

  // Get CdmContext to be used by the media pipeline.
  base::WeakPtr<CdmContext> GetCdmContext();

  int GetCdmIdForTesting() const { return cdm_id_; }

 private:
  void OnInitialized(InitializeCallback callback,
                     ::media::CdmProxy::Status status,
                     ::media::CdmProxy::Protocol protocol,
                     uint32_t crypto_session_id);

  bool has_initialize_been_called_ = false;

  std::unique_ptr<::media::CdmProxy> cdm_proxy_;
  MojoCdmServiceContext* const context_ = nullptr;

  mojo::AssociatedRemote<mojom::CdmProxyClient> client_;

  // Set to a valid CDM ID if the |cdm_proxy_| is successfully initialized.
  int cdm_id_ = CdmContext::kInvalidCdmId;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmProxyService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoCdmProxyService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_SERVICE_H_
