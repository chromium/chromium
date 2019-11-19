// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LIBRARY_CDM_MOCK_LIBRARY_CDM_H_
#define MEDIA_CDM_LIBRARY_CDM_MOCK_LIBRARY_CDM_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "media/cdm/api/content_decryption_module.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class CdmHostProxy;

// Mock implementation of the cdm::ContentDecryptionModule interfaces.
class MockLibraryCdm : public cdm::ContentDecryptionModule_10,
                       public cdm::ContentDecryptionModule_11 {
 public:
  // Provides easy access to the MockLibraryCdm instance for testing to avoid
  // going through multiple layers to get it (e.g. CdmAdapter -> CdmWrapper ->
  // CdmWrapperImpl). It does impose a limitation that we cannot have more than
  // one MockLibraryCdm instances at the same time, which is fine in most
  // testing cases.
  static MockLibraryCdm* GetInstance();

  template <typename HostInterface>
  MockLibraryCdm(HostInterface* host, const std::string& key_system);

  CdmHostProxy* GetCdmHostProxy();

  // cdm::ContentDecryptionModule_10 implementation.
  MOCK_METHOD1(
      InitializeVideoDecoder,
      cdm::Status(const cdm::VideoDecoderConfig_2& video_decoder_config));
  MOCK_METHOD2(DecryptAndDecodeFrame,
               cdm::Status(const cdm::InputBuffer_2& encrypted_buffer,
                           cdm::VideoFrame* video_frame));

  // cdm::ContentDecryptionModule_11 implementation.
  MOCK_METHOD1(
      InitializeVideoDecoder,
      cdm::Status(const cdm::VideoDecoderConfig_3& video_decoder_config));
  MOCK_METHOD2(DecryptAndDecodeFrame,
               cdm::Status(const cdm::InputBuffer_2& encrypted_buffer,
                           cdm::VideoFrame_2* video_frame));

  // cdm::ContentDecryptionModule_10/11 implementation.
  void Initialize(bool allow_distinctive_identifier,
                  bool allow_persistent_state,
                  bool use_hw_secure_codecs) override;
  MOCK_METHOD1(
      InitializeAudioDecoder,
      cdm::Status(const cdm::AudioDecoderConfig_2& audio_decoder_config));
  MOCK_METHOD2(Decrypt,
               cdm::Status(const cdm::InputBuffer_2& encrypted_buffer,
                           cdm::DecryptedBlock* decrypted_block));
  MOCK_METHOD2(DecryptAndDecodeSamples,
               cdm::Status(const cdm::InputBuffer_2& encrypted_buffer,
                           cdm::AudioFrames* audio_frames));

  // Common cdm::ContentDecryptionModule_* implementation.
  MOCK_METHOD2(GetStatusForPolicy,
               void(uint32_t promise_id, const cdm::Policy& policy));
  MOCK_METHOD5(CreateSessionAndGenerateRequest,
               void(uint32_t promise_id,
                    cdm::SessionType session_type,
                    cdm::InitDataType init_data_type,
                    const uint8_t* init_data,
                    uint32_t init_data_size));
  MOCK_METHOD4(LoadSession,
               void(uint32_t promise_id,
                    cdm::SessionType session_type,
                    const char* session_id,
                    uint32_t session_id_length));
  MOCK_METHOD5(UpdateSession,
               void(uint32_t promise_id,
                    const char* session_id,
                    uint32_t session_id_length,
                    const uint8_t* response,
                    uint32_t response_size));
  MOCK_METHOD3(CloseSession,
               void(uint32_t promise_id,
                    const char* session_id,
                    uint32_t session_id_length));
  MOCK_METHOD3(RemoveSession,
               void(uint32_t promise_id,
                    const char* session_id,
                    uint32_t session_id_length));
  MOCK_METHOD3(SetServerCertificate,
               void(uint32_t promise_id,
                    const uint8_t* server_certificate_data,
                    uint32_t server_certificate_data_size));
  MOCK_METHOD1(TimerExpired, void(void* context));
  MOCK_METHOD1(DeinitializeDecoder, void(cdm::StreamType decoder_type));
  MOCK_METHOD1(ResetDecoder, void(cdm::StreamType decoder_type));
  MOCK_METHOD1(OnPlatformChallengeResponse,
               void(const cdm::PlatformChallengeResponse& response));
  MOCK_METHOD3(OnQueryOutputProtectionStatus,
               void(cdm::QueryResult result,
                    uint32_t link_mask,
                    uint32_t output_protection_mask));
  MOCK_METHOD3(OnStorageId,
               void(uint32_t version,
                    const uint8_t* storage_id,
                    uint32_t storage_id_size));

  // It could be tricky to expect Destroy() to be called and then delete
  // MockLibraryCdm directly in the test. So call "delete this" in this class,
  // same as a normal CDM implementation would do, but also add DestroyCalled()
  // so that it's easy to ensure Destroy() is actually called.
  MOCK_METHOD0(DestroyCalled, void());
  void Destroy() override {
    DestroyCalled();
    delete this;
  }

 private:
  // Can only be destructed through Destroy().
  ~MockLibraryCdm() override;

  std::unique_ptr<CdmHostProxy> cdm_host_proxy_;
};

// Helper function to create MockLibraryCdm.
void* CreateMockLibraryCdm(int cdm_interface_version,
                           const char* key_system,
                           uint32_t key_system_size,
                           GetCdmHostFunc get_cdm_host_func,
                           void* user_data);

}  // namespace media

#endif  // MEDIA_CDM_LIBRARY_CDM_MOCK_LIBRARY_CDM_H_
