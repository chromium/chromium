// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/library_cdm/clear_key_cdm/cdm_proxy_handler.h"

#include <stdint.h>
#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/cdm/library_cdm/clear_key_cdm/cdm_proxy_common.h"

namespace media {

CdmProxyHandler::CdmProxyHandler(CdmHostProxy* cdm_host_proxy)
    : cdm_host_proxy_(cdm_host_proxy) {}

CdmProxyHandler::~CdmProxyHandler() {}

void CdmProxyHandler::Initialize(InitCB init_cb) {
  DVLOG(1) << __func__;
  init_cb_ = std::move(init_cb);

  cdm_proxy_ = cdm_host_proxy_->RequestCdmProxy(this);
  if (!cdm_proxy_) {
    FinishInitialization(false);
    return;
  }

  cdm_proxy_->Initialize();
}

void CdmProxyHandler::SetKey(const std::vector<uint8_t>& response,
                             SetKeyCB set_key_cb) {
  DVLOG(2) << __func__;
  DCHECK(!set_key_cb_);
  set_key_cb_ = std::move(set_key_cb);
  cdm_proxy_->SetKey(crypto_session_id_, nullptr, 0,
                     cdm::CdmProxy::kDecryptAndDecode, response.data(),
                     response.size());
}

void CdmProxyHandler::FinishInitialization(bool success) {
  DVLOG(1) << __func__ << ": success = " << success;
  std::move(init_cb_).Run(success);
}

void CdmProxyHandler::OnInitialized(Status status,
                                    Protocol protocol,
                                    uint32_t crypto_session_id) {
  DVLOG(1) << __func__ << ": status = " << status;

  if (status != Status::kOk ||
      crypto_session_id != kClearKeyCdmProxyCryptoSessionId) {
    FinishInitialization(false);
    return;
  }

  // Only one CdmProxy can be created during the lifetime of the CDM instance.
  if (cdm_host_proxy_->RequestCdmProxy(this)) {
    FinishInitialization(false);
    return;
  }

  cdm_proxy_->Process(cdm::CdmProxy::kIntelNegotiateCryptoSessionKeyExchange,
                      crypto_session_id, kClearKeyCdmProxyInputData.data(),
                      kClearKeyCdmProxyInputData.size(), 0);
}

void CdmProxyHandler::OnProcessed(Status status,
                                  const uint8_t* output_data,
                                  uint32_t output_data_size) {
  DVLOG(2) << __func__ << ": status = " << status;

  if (status != Status::kOk ||
      !std::equal(output_data, output_data + output_data_size,
                  kClearKeyCdmProxyOutputData.begin())) {
    FinishInitialization(false);
    return;
  }

  cdm_proxy_->CreateMediaCryptoSession(kClearKeyCdmProxyInputData.data(),
                                       kClearKeyCdmProxyInputData.size());
}

void CdmProxyHandler::OnMediaCryptoSessionCreated(Status status,
                                                  uint32_t crypto_session_id,
                                                  uint64_t output_data) {
  DVLOG(2) << __func__ << ": status = " << status;

  if (status != Status::kOk ||
      crypto_session_id != kClearKeyCdmProxyMediaCryptoSessionId) {
    FinishInitialization(false);
    return;
  }

  FinishInitialization(true);
}

void CdmProxyHandler::OnKeySet(Status status) {
  DVLOG(2) << __func__ << ": status = " << status;
  DCHECK(set_key_cb_);

  std::move(set_key_cb_).Run(status == Status::kOk);
}

void CdmProxyHandler::OnKeyRemoved(Status status) {
  DVLOG(2) << __func__;
  NOTREACHED();
}

void CdmProxyHandler::NotifyHardwareReset() {
  DVLOG(1) << __func__;
  NOTREACHED();
}

}  // namespace media
