// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service_broker.h"

#include <utility>

#include "base/logging.h"
#include "media/cdm/cdm_module.h"
#include "media/media_buildflags.h"

#if defined(OS_MAC)
#include <vector>
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MAC)

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
#if defined(OS_MAC)
    mojo::PendingRemote<mojom::SeatbeltExtensionTokenProvider> token_provider,
#endif  // defined(OS_MAC)
    mojo::PendingReceiver<mojom::CdmService> service_receiver) {
  if (!client_) {
    DVLOG(1) << __func__ << ": CdmService can only be bound once";
    return;
  }

#if defined(OS_MAC)
  InitializeAndEnsureSandboxed(cdm_path, std::move(token_provider));
#else
  InitializeAndEnsureSandboxed(cdm_path);
#endif  // defined(OS_MAC)

  DCHECK(!cdm_service_);
  cdm_service_ = std::make_unique<CdmService>(std::move(client_),
                                              std::move(service_receiver));
}

#if defined(OS_MAC)
void CdmServiceBroker::InitializeAndEnsureSandboxed(
    const base::FilePath& cdm_path,
    mojo::PendingRemote<mojom::SeatbeltExtensionTokenProvider> token_provider) {
#else
void CdmServiceBroker::InitializeAndEnsureSandboxed(
    const base::FilePath& cdm_path) {
#endif  // defined(OS_MAC)
  DVLOG(1) << __func__ << ": cdm_path = " << cdm_path.value();
  DCHECK(client_);

  CdmModule* instance = CdmModule::GetInstance();
  if (instance->was_initialize_called()) {
    DCHECK_EQ(cdm_path, instance->GetCdmPath());
    return;
  }

#if defined(OS_MAC)
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
#endif  // defined(OS_MAC)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  std::vector<CdmHostFilePath> cdm_host_file_paths;
  client_->AddCdmHostFilePaths(&cdm_host_file_paths);
  bool success = instance->Initialize(cdm_path, cdm_host_file_paths);
#else
  bool success = instance->Initialize(cdm_path);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  // This may trigger the sandbox to be sealed.
  client_->EnsureSandboxed();

#if defined(OS_MAC)
  for (auto&& extension : extensions)
    extension->Revoke();
#endif  // defined(OS_MAC)

  // Always called within the sandbox.
  if (success)
    instance->InitializeCdmModule();
}

}  // namespace media
