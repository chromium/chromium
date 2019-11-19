// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/mojo/mojom/cdm_proxy.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// Implements a cdm::CdmProxy that communicates with mojom::CdmProxy.
class MEDIA_MOJO_EXPORT MojoCdmProxy : public cdm::CdmProxy,
                                       mojom::CdmProxyClient {
 public:
  MojoCdmProxy(mojo::PendingRemote<mojom::CdmProxy> cdm_proxy_remote,
               cdm::CdmProxyClient* client);
  ~MojoCdmProxy() override;

  // cdm::CdmProxy implementation.
  void Initialize() final;
  void Process(Function function,
               uint32_t crypto_session_id,
               const uint8_t* input_data,
               uint32_t input_data_size,
               uint32_t expected_output_data_size) final;
  void CreateMediaCryptoSession(const uint8_t* input_data,
                                uint32_t input_data_size) final;
  void SetKey(uint32_t crypto_session_id,
              const uint8_t* key_id,
              uint32_t key_id_size,
              KeyType key_type,
              const uint8_t* key_blob,
              uint32_t key_blob_size) final;
  void RemoveKey(uint32_t crypto_session_id,
                 const uint8_t* key_id,
                 uint32_t key_id_size) final;

  // mojom::CdmProxyClient implementation.
  void NotifyHardwareReset() final;

  // Returns the CDM ID associated with the remote CdmProxy.
  int GetCdmId();

 private:
  void OnInitialized(media::CdmProxy::Status status,
                     media::CdmProxy::Protocol protocol,
                     uint32_t crypto_session_id,
                     int cdm_id);
  void OnProcessed(media::CdmProxy::Status status,
                   const std::vector<uint8_t>& output_data);
  void OnMediaCryptoSessionCreated(media::CdmProxy::Status status,
                                   uint32_t crypto_session_id,
                                   uint64_t output_data);
  void OnKeySet(media::CdmProxy::Status status);
  void OnKeyRemoved(media::CdmProxy::Status status);

  mojo::Remote<mojom::CdmProxy> cdm_proxy_remote_;
  cdm::CdmProxyClient* client_;

  mojo::AssociatedBinding<mojom::CdmProxyClient> client_binding_;

  int cdm_id_ = CdmContext::kInvalidCdmId;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoCdmProxy);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_H_
