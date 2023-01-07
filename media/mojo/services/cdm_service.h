// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_CDM_SERVICE_H_
#define MEDIA_MOJO_SERVICES_CDM_SERVICE_H_

#include <memory>

#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/services/deferred_destroy_unique_receiver_set.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif

namespace media {

class CdmFactory;

class MEDIA_MOJO_EXPORT CdmService final : public mojom::CdmService {
 public:
  class Client {
   public:
    virtual ~Client() {}

    // Called by the MediaService to ensure the process is sandboxed. It could
    // be a no-op if the process is already sandboxed.
    virtual void EnsureSandboxed() = 0;

    // Returns the CdmFactory to be used by MojoCdmService. |frame_interfaces|
    // can be used to request interfaces provided remotely by the host. It may
    // be a nullptr if the host chose not to bind the InterfacePtr.
    virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
        mojom::FrameInterfaceFactory* frame_interfaces) = 0;

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
    // Gets a list of CDM host file paths and put them in |cdm_host_file_paths|.
    virtual void AddCdmHostFilePaths(
        std::vector<CdmHostFilePath>* cdm_host_file_paths) = 0;
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  };

  CdmService(std::unique_ptr<Client> client,
             mojo::PendingReceiver<mojom::CdmService> receiver);
  CdmService(const CdmService&) = delete;
  CdmService operator=(const CdmService&) = delete;
  ~CdmService() final;

  size_t BoundCdmFactorySizeForTesting() const {
    return cdm_factory_receivers_.size();
  }

  size_t UnboundCdmFactorySizeForTesting() const {
    return cdm_factory_receivers_.unbound_size();
  }

  // mojom::CdmService implementation.
  void CreateCdmFactory(
      mojo::PendingReceiver<mojom::CdmFactory> receiver,
      mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) final;

 private:
  mojo::Receiver<mojom::CdmService> receiver_;
  std::unique_ptr<Client> client_;
  std::unique_ptr<CdmFactory> cdm_factory_;
  DeferredDestroyUniqueReceiverSet<mojom::CdmFactory> cdm_factory_receivers_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_CDM_SERVICE_H_
