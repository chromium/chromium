// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_CDM_SERVICE_BROKER_H_
#define MEDIA_MOJO_SERVICES_CDM_SERVICE_BROKER_H_

#include <memory>

#include "build/build_config.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "media/mojo/services/cdm_service.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class MEDIA_MOJO_EXPORT CdmServiceBroker final
    : public mojom::CdmServiceBroker {
 public:
  CdmServiceBroker(std::unique_ptr<CdmService::Client> client,
                   mojo::PendingReceiver<mojom::CdmServiceBroker> receiver);
  CdmServiceBroker(const CdmServiceBroker&) = delete;
  CdmServiceBroker operator=(const CdmServiceBroker&) = delete;
  ~CdmServiceBroker() final;

  // mojom::CdmServiceBroker implementation:
  void GetService(
      const base::FilePath& cdm_path,
#if BUILDFLAG(IS_MAC)
      mojo::PendingRemote<mojom::SeatbeltExtensionTokenProvider> token_provider,
#endif  // BUILDFLAG(IS_MAC)
      mojo::PendingReceiver<mojom::CdmService> service_receiver) final;

 private:
  // Initializes CdmModule and make sure the sandbox is sealed. Returns whether
  // the initialization succeeded or not. In all cases, the process is sandboxed
  // after this call.
  bool InitializeAndEnsureSandboxed(
#if BUILDFLAG(IS_MAC)
      mojo::PendingRemote<mojom::SeatbeltExtensionTokenProvider> token_provider,
#endif  // BUILDFLAG(IS_MAC)
      const base::FilePath& cdm_path);

  std::unique_ptr<CdmService::Client> client_;
  mojo::Receiver<mojom::CdmServiceBroker> receiver_;
  std::unique_ptr<CdmService> cdm_service_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_CDM_SERVICE_BROKER_H_
