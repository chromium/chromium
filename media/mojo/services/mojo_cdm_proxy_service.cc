// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_proxy_service.h"

#include "base/bind.h"
#include "base/macros.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

MojoCdmProxyService::MojoCdmProxyService(
    std::unique_ptr<::media::CdmProxy> cdm_proxy,
    MojoCdmServiceContext* context)
    : cdm_proxy_(std::move(cdm_proxy)), context_(context) {
  DVLOG(1) << __func__;
  DCHECK(cdm_proxy_);
  DCHECK(context_);
}

MojoCdmProxyService::~MojoCdmProxyService() {
  DVLOG(1) << __func__;

  if (cdm_id_ != CdmContext::kInvalidCdmId)
    context_->UnregisterCdmProxy(cdm_id_);
}

void MojoCdmProxyService::Initialize(
    mojo::PendingAssociatedRemote<mojom::CdmProxyClient> client,
    InitializeCallback callback) {
  DVLOG(2) << __func__;

  CHECK(!has_initialize_been_called_) << "Initialize should only happen once";
  has_initialize_been_called_ = true;

  client_.Bind(std::move(client));

  cdm_proxy_->Initialize(
      this, base::BindOnce(&MojoCdmProxyService::OnInitialized,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MojoCdmProxyService::Process(media::CdmProxy::Function function,
                                  uint32_t crypto_session_id,
                                  const std::vector<uint8_t>& input_data,
                                  uint32_t expected_output_data_size,
                                  ProcessCallback callback) {
  DVLOG(3) << __func__;
  cdm_proxy_->Process(function, crypto_session_id, input_data,
                      expected_output_data_size, std::move(callback));
}

void MojoCdmProxyService::CreateMediaCryptoSession(
    const std::vector<uint8_t>& input_data,
    CreateMediaCryptoSessionCallback callback) {
  DVLOG(3) << __func__;
  cdm_proxy_->CreateMediaCryptoSession(input_data, std::move(callback));
}

void MojoCdmProxyService::SetKey(uint32_t crypto_session_id,
                                 const std::vector<uint8_t>& key_id,
                                 media::CdmProxy::KeyType key_type,
                                 const std::vector<uint8_t>& key_blob,
                                 SetKeyCallback callback) {
  DVLOG(3) << __func__;
  cdm_proxy_->SetKey(crypto_session_id, key_id, key_type, key_blob,
                     std::move(callback));
}

void MojoCdmProxyService::RemoveKey(uint32_t crypto_session_id,
                                    const std::vector<uint8_t>& key_id,
                                    RemoveKeyCallback callback) {
  DVLOG(3) << __func__;
  cdm_proxy_->RemoveKey(crypto_session_id, key_id, std::move(callback));
}

void MojoCdmProxyService::NotifyHardwareReset() {
  DVLOG(2) << __func__;
  client_->NotifyHardwareReset();
}

base::WeakPtr<CdmContext> MojoCdmProxyService::GetCdmContext() {
  DVLOG(2) << __func__;
  return cdm_proxy_->GetCdmContext();
}

void MojoCdmProxyService::OnInitialized(InitializeCallback callback,
                                        ::media::CdmProxy::Status status,
                                        ::media::CdmProxy::Protocol protocol,
                                        uint32_t crypto_session_id) {
  CHECK_EQ(cdm_id_, CdmContext::kInvalidCdmId)
      << "CDM proxy should only be created once.";

  if (status == ::media::CdmProxy::Status::kOk)
    cdm_id_ = context_->RegisterCdmProxy(this);

  std::move(callback).Run(status, protocol, crypto_session_id, cdm_id_);
}

}  // namespace media
