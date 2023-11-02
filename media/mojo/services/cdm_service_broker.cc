// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service_broker.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "media/cdm/cdm_module.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_MAC)
#include <vector>
#include "sandbox/mac/seatbelt_extension.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif

namespace media {

CdmServiceBroker::CdmServiceBroker(
    std::unique_ptr<CdmService::Client> client,
    mojo::PendingReceiver<mojom::CdmServiceBroker> receiver)
    : client_(std::move(client)), receiver_(this, std::move(receiver)) {
  DVLOG(1) << __func__;
  DCHECK(client_);
}

CdmServiceBroker::~CdmServiceBroker() = default;

void CdmServiceBroker::GetService(
    const base::FilePath& cdm_path,
#if BUILDFLAG(IS_MAC)
    mojo::PendingRemote<mojom::SeatbeltExtensionTokenProvider> token_provider,
#endif  // BUILDFLAG(IS_MAC)
    mojo::PendingReceiver<mojom::CdmService> service_receiver) {
  if (!client_) {
    DVLOG(1) << __func__ << ": CdmService can only be bound once";
    return;
  }

  bool success = InitializeAndEnsureSandboxed(
#if BUILDFLAG(IS_MAC)
      std::move(token_provider),
#endif  // BUILDFLAG(IS_MAC)
      cdm_path);

  if (!success) {
    client_.reset();
    return;
  }

  DCHECK(!cdm_service_);
  cdm_service_ = std::make_unique<CdmService>(std::move(client_),
                                              std::move(service_receiver));
}

bool CdmServiceBroker::InitializeAndEnsureSandboxed(
#if BUILDFLAG(IS_MAC)
    mojo::PendingRemote<mojom::SeatbeltExtensionTokenProvider> token_provider,
#endif  // BUILDFLAG(IS_MAC)
    const base::FilePath& cdm_path) {
  DVLOG(1) << __func__ << ": cdm_path = " << cdm_path.value();
  DCHECK(client_);

#if BUILDFLAG(IS_MAC)
  std::vector<std::unique_ptr<sandbox::SeatbeltExtension>> extensions;

  if (token_provider) {
    std::vector<sandbox::SeatbeltExtensionToken> tokens;
    CHECK(mojo::Remote<mojom::SeatbeltExtensionTokenProvider>(
              std::move(token_provider))
              ->GetTokens(&tokens));

    for (auto&& token : tokens) {
      DVLOG(3) << "token: " << token.token();
      auto extension = sandbox::SeatbeltExtension::FromToken(std::move(token));
      if (!extension->Consume()) {
        DVLOG(1) << "Failed to consume sandbox seatbelt extension. This could "
                    "happen if --no-sandbox is specified.";
      }
      extensions.push_back(std::move(extension));
    }
  }
#endif  // BUILDFLAG(IS_MAC)

  CdmModule* instance = CdmModule::GetInstance();

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  std::vector<CdmHostFilePath> cdm_host_file_paths;
  client_->AddCdmHostFilePaths(&cdm_host_file_paths);
  bool success = instance->Initialize(cdm_path, cdm_host_file_paths);
#else
  bool success = instance->Initialize(cdm_path);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  // This may trigger the sandbox to be sealed. After this call, the process is
  // sandboxed.
  client_->EnsureSandboxed();

#if BUILDFLAG(IS_MAC)
  for (auto&& extension : extensions)
    extension->Revoke();
#endif  // BUILDFLAG(IS_MAC)

  // Always called within the sandbox.
  if (success)
    instance->InitializeCdmModule();

  return success;
}

}  // namespace media
