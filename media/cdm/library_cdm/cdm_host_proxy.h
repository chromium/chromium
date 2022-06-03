// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LIBRARY_CDM_CDM_HOST_PROXY_H_
#define MEDIA_CDM_LIBRARY_CDM_CDM_HOST_PROXY_H_

#include "media/cdm/api/content_decryption_module.h"

namespace media {

// An interface to proxy calls to the CDM Host.
class CdmHostProxy {
 public:
  virtual ~CdmHostProxy() = default;

  // This needs to be a superset of all Host interfaces supported.
  virtual void OnInitialized(bool success) = 0;
  virtual cdm::Buffer* Allocate(uint32_t capacity) = 0;
  virtual void SetTimer(int64_t delay_ms, void* context) = 0;
  virtual cdm::Time GetCurrentWallTime() = 0;
  virtual void OnResolveKeyStatusPromise(uint32_t promise_id,
                                         cdm::KeyStatus key_status) = 0;
  virtual void OnResolveNewSessionPromise(uint32_t promise_id,
                                          const char* session_id,
                                          uint32_t session_id_size) = 0;
  virtual void OnResolvePromise(uint32_t promise_id) = 0;
  virtual void OnRejectPromise(uint32_t promise_id,
                               cdm::Exception exception,
                               uint32_t system_code,
                               const char* error_message,
                               uint32_t error_message_size) = 0;
  virtual void OnSessionMessage(const char* session_id,
                                uint32_t session_id_size,
                                cdm::MessageType message_type,
                                const char* message,
                                uint32_t message_size) = 0;
  virtual void OnSessionKeysChange(const char* session_id,
                                   uint32_t session_id_size,
                                   bool has_additional_usable_key,
                                   const cdm::KeyInformation* keys_info,
                                   uint32_t keys_info_count) = 0;
  virtual void OnExpirationChange(const char* session_id,
                                  uint32_t session_id_size,
                                  cdm::Time new_expiry_time) = 0;
  virtual void OnSessionClosed(const char* session_id,
                               uint32_t session_id_size) = 0;
  virtual void SendPlatformChallenge(const char* service_id,
                                     uint32_t service_id_size,
                                     const char* challenge,
                                     uint32_t challenge_size) = 0;
  virtual void EnableOutputProtection(uint32_t desired_protection_mask) = 0;
  virtual void QueryOutputProtectionStatus() = 0;
  virtual void OnDeferredInitializationDone(cdm::StreamType stream_type,
                                            cdm::Status decoder_status) = 0;
  virtual cdm::FileIO* CreateFileIO(cdm::FileIOClient* client) = 0;
  virtual void RequestStorageId(uint32_t version) = 0;
};

}  // namespace media

#endif  // MEDIA_CDM_LIBRARY_CDM_CDM_HOST_PROXY_H_
