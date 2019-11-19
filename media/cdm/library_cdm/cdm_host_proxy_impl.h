// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LIBRARY_CDM_CDM_HOST_PROXY_IMPL_H_
#define MEDIA_CDM_LIBRARY_CDM_CDM_HOST_PROXY_IMPL_H_

#include "media/cdm/library_cdm/cdm_host_proxy.h"

#include "base/logging.h"
#include "base/macros.h"

namespace media {

// A templated implementation of CdmHostProxy to forward Host calls to the
// correct CDM Host.
template <typename HostInterface>
class CdmHostProxyImpl : public CdmHostProxy {
 public:
  explicit CdmHostProxyImpl(HostInterface* host) : host_(host) {}
  ~CdmHostProxyImpl() override {}

  void OnInitialized(bool success) final {
    return host_->OnInitialized(success);
  }

  cdm::Buffer* Allocate(uint32_t capacity) final {
    return host_->Allocate(capacity);
  }

  void SetTimer(int64_t delay_ms, void* context) final {
    host_->SetTimer(delay_ms, context);
  }

  cdm::Time GetCurrentWallTime() final { return host_->GetCurrentWallTime(); }

  void OnResolveKeyStatusPromise(uint32_t promise_id,
                                 cdm::KeyStatus key_status) final {
    host_->OnResolveKeyStatusPromise(promise_id, key_status);
  }

  void OnResolveNewSessionPromise(uint32_t promise_id,
                                  const char* session_id,
                                  uint32_t session_id_size) final {
    host_->OnResolveNewSessionPromise(promise_id, session_id, session_id_size);
  }

  void OnResolvePromise(uint32_t promise_id) final {
    host_->OnResolvePromise(promise_id);
  }

  void OnRejectPromise(uint32_t promise_id,
                       cdm::Exception exception,
                       uint32_t system_code,
                       const char* error_message,
                       uint32_t error_message_size) final {
    host_->OnRejectPromise(promise_id, exception, system_code, error_message,
                           error_message_size);
  }

  void OnSessionMessage(const char* session_id,
                        uint32_t session_id_size,
                        cdm::MessageType message_type,
                        const char* message,
                        uint32_t message_size) final {
    host_->OnSessionMessage(session_id, session_id_size, message_type, message,
                            message_size);
  }

  void OnSessionKeysChange(const char* session_id,
                           uint32_t session_id_size,
                           bool has_additional_usable_key,
                           const cdm::KeyInformation* keys_info,
                           uint32_t keys_info_count) final {
    host_->OnSessionKeysChange(session_id, session_id_size,
                               has_additional_usable_key, keys_info,
                               keys_info_count);
  }

  void OnExpirationChange(const char* session_id,
                          uint32_t session_id_size,
                          cdm::Time new_expiry_time) final {
    host_->OnExpirationChange(session_id, session_id_size, new_expiry_time);
  }

  void OnSessionClosed(const char* session_id, uint32_t session_id_size) final {
    host_->OnSessionClosed(session_id, session_id_size);
  }

  void SendPlatformChallenge(const char* service_id,
                             uint32_t service_id_size,
                             const char* challenge,
                             uint32_t challenge_size) final {
    host_->SendPlatformChallenge(service_id, service_id_size, challenge,
                                 challenge_size);
  }

  void EnableOutputProtection(uint32_t desired_protection_mask) final {
    host_->EnableOutputProtection(desired_protection_mask);
  }

  void QueryOutputProtectionStatus() final {
    host_->QueryOutputProtectionStatus();
  }

  void OnDeferredInitializationDone(cdm::StreamType stream_type,
                                    cdm::Status decoder_status) final {}

  cdm::FileIO* CreateFileIO(cdm::FileIOClient* client) final {
    return host_->CreateFileIO(client);
  }

  cdm::CdmProxy* RequestCdmProxy(cdm::CdmProxyClient* client) final {
    return host_->RequestCdmProxy(client);
  }

  void RequestStorageId(uint32_t version) final {
    host_->RequestStorageId(version);
  }

 private:
  HostInterface* const host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CdmHostProxyImpl);
};

// Specialization for cdm::Host_10 methods.

template <>
cdm::CdmProxy* CdmHostProxyImpl<cdm::Host_10>::RequestCdmProxy(
    cdm::CdmProxyClient* /* client */) {
  NOTREACHED() << "cdm::ContentDecryptionModule_10 CDM should never call this.";
  return nullptr;
}

}  // namespace media

#endif  // MEDIA_CDM_LIBRARY_CDM_CDM_HOST_PROXY_IMPL_H_
