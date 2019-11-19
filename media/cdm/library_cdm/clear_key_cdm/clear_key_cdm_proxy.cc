// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/library_cdm/clear_key_cdm/clear_key_cdm_proxy.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/base/content_decryption_module.h"
#include "media/cdm/library_cdm/clear_key_cdm/cdm_proxy_common.h"

namespace media {

namespace {

constexpr char kDummySessionId[] = "dummy session id";

class IgnoreResponsePromise : public SimpleCdmPromise {
 public:
  IgnoreResponsePromise() = default;
  ~IgnoreResponsePromise() override = default;

  // SimpleCdmPromise implementation.
  void resolve() final { MarkPromiseSettled(); }
  void reject(CdmPromise::Exception exception_code,
              uint32_t system_code,
              const std::string& error_message) final {
    MarkPromiseSettled();
  }
};

}  // namespace

ClearKeyCdmProxy::ClearKeyCdmProxy() {}

ClearKeyCdmProxy::~ClearKeyCdmProxy() {}

base::WeakPtr<CdmContext> ClearKeyCdmProxy::GetCdmContext() {
  DVLOG(1) << __func__;
  return weak_factory_.GetWeakPtr();
}

void ClearKeyCdmProxy::Initialize(Client* client, InitializeCB init_cb) {
  DVLOG(1) << __func__;

  std::move(init_cb).Run(Status::kOk, Protocol::kIntel,
                         kClearKeyCdmProxyCryptoSessionId);
}

void ClearKeyCdmProxy::Process(Function function,
                               uint32_t crypto_session_id,
                               const std::vector<uint8_t>& input_data,
                               uint32_t expected_output_data_size,
                               ProcessCB process_cb) {
  DVLOG(2) << __func__;

  if (crypto_session_id != kClearKeyCdmProxyCryptoSessionId ||
      !std::equal(input_data.begin(), input_data.end(),
                  kClearKeyCdmProxyInputData.begin(),
                  kClearKeyCdmProxyInputData.end())) {
    std::move(process_cb).Run(Status::kFail, {});
    return;
  }

  std::move(process_cb)
      .Run(Status::kOk,
           std::vector<uint8_t>(kClearKeyCdmProxyOutputData.begin(),
                                kClearKeyCdmProxyOutputData.end()));
}

void ClearKeyCdmProxy::CreateMediaCryptoSession(
    const std::vector<uint8_t>& input_data,
    CreateMediaCryptoSessionCB create_media_crypto_session_cb) {
  DVLOG(2) << __func__;

  if (!std::equal(input_data.begin(), input_data.end(),
                  kClearKeyCdmProxyInputData.begin(),
                  kClearKeyCdmProxyInputData.end())) {
    std::move(create_media_crypto_session_cb).Run(Status::kFail, 0, 0);
    return;
  }

  std::move(create_media_crypto_session_cb)
      .Run(Status::kOk, kClearKeyCdmProxyMediaCryptoSessionId, 0);
}

void ClearKeyCdmProxy::SetKey(uint32_t crypto_session_id,
                              const std::vector<uint8_t>& key_id,
                              KeyType /* key_type */,
                              const std::vector<uint8_t>& key_blob,
                              SetKeyCB set_key_cb) {
  DVLOG(1) << __func__;

  if (!aes_decryptor_)
    CreateDecryptor();

  aes_decryptor_->UpdateSession(kDummySessionId, key_blob,
                                std::make_unique<IgnoreResponsePromise>());
  std::move(set_key_cb).Run(Status::kOk);
}

void ClearKeyCdmProxy::RemoveKey(uint32_t crypto_session_id,
                                 const std::vector<uint8_t>& key_id,
                                 RemoveKeyCB remove_key_cb) {
  std::move(remove_key_cb).Run(Status::kOk);
}

Decryptor* ClearKeyCdmProxy::GetDecryptor() {
  DVLOG(1) << __func__;

  if (!aes_decryptor_)
    CreateDecryptor();

  return aes_decryptor_.get();
}

void ClearKeyCdmProxy::CreateDecryptor() {
  DVLOG(1) << __func__;
  DCHECK(!aes_decryptor_);

  aes_decryptor_ =
      base::MakeRefCounted<AesDecryptor>(base::DoNothing(), base::DoNothing(),
                                         base::DoNothing(), base::DoNothing());

  // Also create a dummy session to be used for SetKey().
  aes_decryptor_->CreateSession(kDummySessionId, CdmSessionType::kTemporary);
}

}  // namespace media
