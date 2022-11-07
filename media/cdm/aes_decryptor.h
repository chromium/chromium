// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_AES_DECRYPTOR_H_
#define MEDIA_CDM_AES_DECRYPTOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"
#include "media/cdm/json_web_key.h"

namespace crypto {
class SymmetricKey;
}

namespace media {

// Decrypts an AES encrypted buffer into an unencrypted buffer. The AES
// encryption must be CTR with a key size of 128bits.
class MEDIA_EXPORT AesDecryptor : public ContentDecryptionModule,
                                  public CdmContext,
                                  public Decryptor {
 public:
  AesDecryptor(const SessionMessageCB& session_message_cb,
               const SessionClosedCB& session_closed_cb,
               const SessionKeysChangeCB& session_keys_change_cb,
               const SessionExpirationUpdateCB& session_expiration_update_cb);

  AesDecryptor(const AesDecryptor&) = delete;
  AesDecryptor& operator=(const AesDecryptor&) = delete;

  // ContentDecryptionModule implementation.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) override;
  void LoadSession(CdmSessionType session_type,
                   const std::string& session_id,
                   std::unique_ptr<NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  CdmContext* GetCdmContext() override;

  // CdmContext implementation.
  std::unique_ptr<CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override;
  Decryptor* GetDecryptor() override;

  // Decryptor implementation.
  void Decrypt(StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               DecryptCB decrypt_cb) override;
  void CancelDecrypt(StreamType stream_type) override;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              DecoderInitCB init_cb) override;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              DecoderInitCB init_cb) override;
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             AudioDecodeCB audio_decode_cb) override;
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             VideoDecodeCB video_decode_cb) override;
  void ResetDecoder(StreamType stream_type) override;
  void DeinitializeDecoder(StreamType stream_type) override;
  bool CanAlwaysDecrypt() override;

 private:
  // Testing classes that needs to manipulate internal states for testing.
  friend class ClearKeyPersistentSessionCdm;

  // Internally this class supports persistent license type sessions so that
  // it can be used by ClearKeyPersistentSessionCdm. The following methods
  // will be used from ClearKeyPersistentSessionCdm to create and update
  // persistent sessions. Note that ClearKeyPersistentSessionCdm is only used
  // for testing, so persistent sessions will not be available generally.

  // Creates a new session with ID |session_id| and type |session_type|, and
  // adds it to the list of active sessions. Returns false if the session ID
  // is already in the list.
  bool CreateSession(const std::string& session_id,
                     CdmSessionType session_type);

  // Gets the state of the session |session_id| as a JWK.
  std::string GetSessionStateAsJWK(const std::string& session_id);

  // Update session |session_id| with the JWK provided in |json_web_key_set|.
  // Returns true and sets |key_added| if successful, otherwise returns false
  // and |error_message| is the reason for failure.
  bool UpdateSessionWithJWK(const std::string& session_id,
                            const std::string& json_web_key_set,
                            bool* key_added,
                            CdmPromise::Exception* exception,
                            std::string* error_message);

  // Performs the final steps of UpdateSession (notify any listeners for keys
  // changed, resolve the promise, and generate a keys change event).
  void FinishUpdate(const std::string& session_id,
                    bool key_added,
                    std::unique_ptr<SimpleCdmPromise> promise);

  // TODO(fgalligan): Remove this and change KeyMap to use crypto::SymmetricKey
  // as there are no decryptors that are performing an integrity check.
  // Helper class that manages the decryption key.
  class DecryptionKey {
   public:
    explicit DecryptionKey(const std::string& secret);

    DecryptionKey(const DecryptionKey&) = delete;
    DecryptionKey& operator=(const DecryptionKey&) = delete;

    ~DecryptionKey();

    // Creates the encryption key.
    bool Init();

    const std::string& secret() { return secret_; }
    crypto::SymmetricKey* decryption_key() { return decryption_key_.get(); }

   private:
    // The base secret that is used to create the decryption key.
    const std::string secret_;

    // The key used to decrypt the data.
    std::unique_ptr<crypto::SymmetricKey> decryption_key_;
  };

  // Keep track of the keys for a key ID. If multiple sessions specify keys
  // for the same key ID, then the last key inserted is used. The structure is
  // optimized so that Decrypt() has fast access, at the cost of slow deletion
  // of keys when a session is released.
  class SessionIdDecryptionKeyMap;

  // Key ID <-> SessionIdDecryptionKeyMap map.
  using KeyIdToSessionKeysMap =
      std::unordered_map<std::string,
                         std::unique_ptr<SessionIdDecryptionKeyMap>>;

  ~AesDecryptor() override;

  // Creates a DecryptionKey using |key_string| and associates it with |key_id|.
  // Returns true if successful.
  bool AddDecryptionKey(const std::string& session_id,
                        const std::string& key_id,
                        const std::string& key_string);

  // Gets a DecryptionKey associated with |key_id|. The AesDecryptor still owns
  // the key. Returns NULL if no key is associated with |key_id|.
  DecryptionKey* GetKey_Locked(const std::string& key_id) const
      EXCLUSIVE_LOCKS_REQUIRED(key_map_lock_);

  // Determines if |key_id| is already specified for |session_id|.
  bool HasKey(const std::string& session_id, const std::string& key_id);

  // Deletes all keys associated with |session_id|.
  void DeleteKeysForSession(const std::string& session_id);

  CdmKeysInfo GenerateKeysInfoList(const std::string& session_id,
                                   CdmKeyInformation::KeyStatus status);

  // Callbacks for firing session events. No SessionExpirationUpdateCB since
  // the keys never expire.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionKeysChangeCB session_keys_change_cb_;

  // Since only Decrypt() is called off the renderer thread, we only need to
  // protect |key_map_|, the only member variable that is shared between
  // Decrypt() and other methods.
  mutable base::Lock key_map_lock_;
  KeyIdToSessionKeysMap key_map_ GUARDED_BY(key_map_lock_);

  // Keeps track of current open sessions and their type. Although publicly
  // AesDecryptor only supports temporary sessions, ClearKeyPersistentSessionCdm
  // uses this class to also support persistent sessions, so save the
  // CdmSessionType for each session.
  std::map<std::string, CdmSessionType> open_sessions_;

  CallbackRegistry<EventCB::RunType> event_callbacks_;
};

}  // namespace media

#endif  // MEDIA_CDM_AES_DECRYPTOR_H_
