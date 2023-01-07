// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TEST_FAKE_ENCRYPTED_MEDIA_H_
#define MEDIA_TEST_FAKE_ENCRYPTED_MEDIA_H_

#include "base/memory/raw_ptr.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"

namespace media {

class AesDecryptor;

// Note: Tests using this class only exercise the DecryptingDemuxerStream path.
// They do not exercise the Decrypting{Audio|Video}Decoder path.
class FakeEncryptedMedia {
 public:
  // Defines the behavior of the "app" that responds to EME events.
  class AppBase {
   public:
    virtual ~AppBase() {}

    virtual void OnSessionMessage(const std::string& session_id,
                                  CdmMessageType message_type,
                                  const std::vector<uint8_t>& message,
                                  AesDecryptor* decryptor) = 0;

    virtual void OnSessionClosed(const std::string& session_id,
                                 CdmSessionClosedReason reason) = 0;

    virtual void OnSessionKeysChange(const std::string& session_id,
                                     bool has_additional_usable_key,
                                     CdmKeysInfo keys_info) = 0;

    virtual void OnSessionExpirationUpdate(const std::string& session_id,
                                           base::Time new_expiry_time) = 0;

    virtual void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                          const std::vector<uint8_t>& init_data,
                                          AesDecryptor* decryptor) = 0;
  };

  FakeEncryptedMedia(AppBase* app);

  FakeEncryptedMedia(const FakeEncryptedMedia&) = delete;
  FakeEncryptedMedia& operator=(const FakeEncryptedMedia&) = delete;

  ~FakeEncryptedMedia();
  CdmContext* GetCdmContext();

  // Callbacks for firing session events. Delegate to |app_|.
  void OnSessionMessage(const std::string& session_id,
                        CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionClosed(const std::string& session_id,
                       CdmSessionClosedReason reason);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 base::Time new_expiry_time);
  void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);

 private:
  class TestCdmContext : public CdmContext {
   public:
    TestCdmContext(Decryptor* decryptor);
    Decryptor* GetDecryptor() final;

   private:
    raw_ptr<Decryptor> decryptor_;
  };

  scoped_refptr<AesDecryptor> decryptor_;
  TestCdmContext cdm_context_;
  std::unique_ptr<AppBase> app_;
};

}  // namespace media

#endif  // MEDIA_TEST_FAKE_ENCRYPTED_MEDIA_H_
