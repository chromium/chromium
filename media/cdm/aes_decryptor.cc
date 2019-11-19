// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/aes_decryptor.h"

#include <stddef.h>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "crypto/symmetric_key.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_promise.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/limits.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/cdm/cbcs_decryptor.h"
#include "media/cdm/cenc_decryptor.h"
#include "media/cdm/cenc_utils.h"
#include "media/cdm/json_web_key.h"

namespace media {

namespace {

// Vastly simplified ACM random class, based on media/base/test_random.h.
// base/rand_util.h doesn't work in the sandbox. This class generates
// predictable sequences of pseudorandom numbers. These are only used for
// persistent session IDs, so unpredictable sequences are not necessary.
uint32_t Rand(uint32_t seed) {
  static const uint64_t A = 16807;        // bits 14, 8, 7, 5, 2, 1, 0
  static const uint64_t M = 2147483647L;  // 2^32-1
  return static_cast<uint32_t>((seed * A) % M);
}

// Create a random session ID. Returned value is a printable string to make
// logging the session ID easier.
std::string GenerateSessionId() {
  // Create a random value. There is a slight chance that the same ID is
  // generated in different processes, but session IDs are only ever saved
  // by External Clear Key, which is test only.
  static uint32_t seed = 0;
  if (!seed) {
    // If this is the first call, use the current time as the starting value.
    seed = static_cast<uint32_t>(base::Time::Now().ToInternalValue());
  }
  seed = Rand(seed);

  // Include an incrementing value to ensure that the session ID is unique
  // in this process.
  static uint32_t next_session_id_suffix = 0;
  next_session_id_suffix++;

  return base::HexEncode(&seed, sizeof(seed)) +
         base::HexEncode(&next_session_id_suffix,
                         sizeof(next_session_id_suffix));
}

}  // namespace

// Keeps track of the session IDs and DecryptionKeys. The keys are ordered by
// insertion time (last insertion is first). It takes ownership of the
// DecryptionKeys.
class AesDecryptor::SessionIdDecryptionKeyMap {
  // Use a std::list to actually hold the data. Insertion is always done
  // at the front, so the "latest" decryption key is always the first one
  // in the list.
  using KeyList =
      std::list<std::pair<std::string, std::unique_ptr<DecryptionKey>>>;

 public:
  SessionIdDecryptionKeyMap() = default;
  ~SessionIdDecryptionKeyMap() = default;

  // Replaces value if |session_id| is already present, or adds it if not.
  // This |decryption_key| becomes the latest until another insertion or
  // |session_id| is erased.
  void Insert(const std::string& session_id,
              std::unique_ptr<DecryptionKey> decryption_key);

  // Deletes the entry for |session_id| if present.
  void Erase(const std::string& session_id);

  // Returns whether the list is empty
  bool Empty() const { return key_list_.empty(); }

  // Returns the last inserted DecryptionKey.
  DecryptionKey* LatestDecryptionKey() {
    DCHECK(!key_list_.empty());
    return key_list_.begin()->second.get();
  }

  bool Contains(const std::string& session_id) {
    return Find(session_id) != key_list_.end();
  }

 private:
  // Searches the list for an element with |session_id|.
  KeyList::iterator Find(const std::string& session_id);

  // Deletes the entry pointed to by |position|.
  void Erase(KeyList::iterator position);

  KeyList key_list_;

  DISALLOW_COPY_AND_ASSIGN(SessionIdDecryptionKeyMap);
};

void AesDecryptor::SessionIdDecryptionKeyMap::Insert(
    const std::string& session_id,
    std::unique_ptr<DecryptionKey> decryption_key) {
  auto it = Find(session_id);
  if (it != key_list_.end())
    Erase(it);
  key_list_.push_front(std::make_pair(session_id, std::move(decryption_key)));
}

void AesDecryptor::SessionIdDecryptionKeyMap::Erase(
    const std::string& session_id) {
  auto it = Find(session_id);
  if (it == key_list_.end())
    return;
  Erase(it);
}

AesDecryptor::SessionIdDecryptionKeyMap::KeyList::iterator
AesDecryptor::SessionIdDecryptionKeyMap::Find(const std::string& session_id) {
  for (auto it = key_list_.begin(); it != key_list_.end(); ++it) {
    if (it->first == session_id)
      return it;
  }
  return key_list_.end();
}

void AesDecryptor::SessionIdDecryptionKeyMap::Erase(
    KeyList::iterator position) {
  DCHECK(position->second);
  key_list_.erase(position);
}

// Decrypts |input| using |key|.  Returns a DecoderBuffer with the decrypted
// data if decryption succeeded or NULL if decryption failed.
static scoped_refptr<DecoderBuffer> DecryptData(
    const DecoderBuffer& input,
    const crypto::SymmetricKey& key) {
  CHECK(input.data_size());
  CHECK(input.decrypt_config());

  if (input.decrypt_config()->encryption_scheme() == EncryptionScheme::kCenc)
    return DecryptCencBuffer(input, key);

  if (input.decrypt_config()->encryption_scheme() == EncryptionScheme::kCbcs)
    return DecryptCbcsBuffer(input, key);

  DVLOG(1) << "Only 'cenc' and 'cbcs' modes supported.";
  return nullptr;
}

AesDecryptor::AesDecryptor(
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  DVLOG(1) << __func__;
  // AesDecryptor doesn't keep any persistent data, so no need to do anything
  // with |security_origin|.
  DCHECK(session_message_cb_);
  DCHECK(session_closed_cb_);
  DCHECK(session_keys_change_cb_);
}

AesDecryptor::~AesDecryptor() {
  DVLOG(1) << __func__;
  key_map_.clear();
}

void AesDecryptor::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "SetServerCertificate() is not supported.");
}

void AesDecryptor::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  std::string session_id = GenerateSessionId();
  bool session_added = CreateSession(session_id, session_type);
  DCHECK(session_added) << "Failed to add new session " << session_id;

  std::vector<uint8_t> message;
  std::vector<std::vector<uint8_t>> keys;
  switch (init_data_type) {
    case EmeInitDataType::WEBM:
      // |init_data| is simply the key needed.
      if (init_data.size() < limits::kMinKeyIdLength ||
          init_data.size() > limits::kMaxKeyIdLength) {
        promise->reject(CdmPromise::Exception::TYPE_ERROR, 0,
                        "Incorrect length");
        return;
      }
      keys.push_back(init_data);
      break;
    case EmeInitDataType::CENC:
      // |init_data| is a set of 0 or more concatenated 'pssh' boxes.
      if (!GetKeyIdsForCommonSystemId(init_data, &keys)) {
        promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                        "No supported PSSH box found.");
        return;
      }
      break;
    case EmeInitDataType::KEYIDS: {
      std::string init_data_string(init_data.begin(), init_data.end());
      std::string error_message;
      if (!ExtractKeyIdsFromKeyIdsInitData(init_data_string, &keys,
                                           &error_message)) {
        promise->reject(CdmPromise::Exception::TYPE_ERROR, 0, error_message);
        return;
      }
      break;
    }
    default:
      NOTREACHED();
      promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                      "init_data_type not supported.");
      return;
  }
  CreateLicenseRequest(keys, session_type, &message);

  promise->resolve(session_id);

  session_message_cb_.Run(session_id, CdmMessageType::LICENSE_REQUEST, message);
}

void AesDecryptor::LoadSession(CdmSessionType session_type,
                               const std::string& session_id,
                               std::unique_ptr<NewSessionCdmPromise> promise) {
  // LoadSession() is not supported directly, as there is no way to persist
  // the session state. Should not be called as blink should not allow
  // persistent sessions for ClearKey.
  NOTREACHED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "LoadSession() is not supported.");
}

void AesDecryptor::UpdateSession(const std::string& session_id,
                                 const std::vector<uint8_t>& response,
                                 std::unique_ptr<SimpleCdmPromise> promise) {
  CHECK(!response.empty());

  // Currently the EME spec has blink check for session closed synchronously,
  // but then this is called asynchronously. So it is possible that update()
  // could get called on a closed session.
  // https://github.com/w3c/encrypted-media/issues/365
  if (open_sessions_.find(session_id) == open_sessions_.end()) {
    promise->reject(CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "Session does not exist.");
    return;
  }

  bool key_added = false;
  CdmPromise::Exception exception;
  std::string error_message;
  if (!UpdateSessionWithJWK(session_id,
                            std::string(response.begin(), response.end()),
                            &key_added, &exception, &error_message)) {
    promise->reject(exception, 0, error_message);
    return;
  }

  FinishUpdate(session_id, key_added, std::move(promise));
}

bool AesDecryptor::UpdateSessionWithJWK(const std::string& session_id,
                                        const std::string& json_web_key_set,
                                        bool* key_added,
                                        CdmPromise::Exception* exception,
                                        std::string* error_message) {
  auto open_session = open_sessions_.find(session_id);
  DCHECK(open_session != open_sessions_.end());
  CdmSessionType session_type = open_session->second;

  KeyIdAndKeyPairs keys;
  if (!ExtractKeysFromJWKSet(json_web_key_set, &keys, &session_type)) {
    *exception = CdmPromise::Exception::TYPE_ERROR;
    error_message->assign("Invalid JSON Web Key Set.");
    return false;
  }

  // Make sure that at least one key was extracted.
  if (keys.empty()) {
    *exception = CdmPromise::Exception::TYPE_ERROR;
    error_message->assign("JSON Web Key Set does not contain any keys.");
    return false;
  }

  bool local_key_added = false;
  for (auto it = keys.begin(); it != keys.end(); ++it) {
    if (it->second.length() !=
        static_cast<size_t>(DecryptConfig::kDecryptionKeySize)) {
      DVLOG(1) << "Invalid key length: " << it->second.length();
      *exception = CdmPromise::Exception::TYPE_ERROR;
      error_message->assign("Invalid key length.");
      return false;
    }

    // If this key_id doesn't currently exist in this session,
    // a new key is added.
    if (!HasKey(session_id, it->first))
      local_key_added = true;

    if (!AddDecryptionKey(session_id, it->first, it->second)) {
      *exception = CdmPromise::Exception::INVALID_STATE_ERROR;
      error_message->assign("Unable to add key.");
      return false;
    }
  }

  *key_added = local_key_added;
  return true;
}

void AesDecryptor::FinishUpdate(const std::string& session_id,
                                bool key_added,
                                std::unique_ptr<SimpleCdmPromise> promise) {
  {
    base::AutoLock auto_lock(new_key_cb_lock_);

    if (new_audio_key_cb_)
      new_audio_key_cb_.Run();

    if (new_video_key_cb_)
      new_video_key_cb_.Run();
  }

  promise->resolve();

  session_keys_change_cb_.Run(
      session_id, key_added,
      GenerateKeysInfoList(session_id, CdmKeyInformation::USABLE));
}

// Runs the parallel steps from https://w3c.github.io/encrypted-media/#close.
void AesDecryptor::CloseSession(const std::string& session_id,
                                std::unique_ptr<SimpleCdmPromise> promise) {
  // Validate that this is a reference to an open session. close() shouldn't
  // be called if the session is already closed. However, the operation is
  // asynchronous, so there is a window where close() was called a second time
  // just before the closed event arrives. As a result it is possible that the
  // session is already closed, so assume that the session is closed if it
  // doesn't exist. https://github.com/w3c/encrypted-media/issues/365.
  //
  // close() is called from a MediaKeySession object, so it is unlikely that
  // this method will be called with a previously unseen |session_id|.
  auto it = open_sessions_.find(session_id);
  if (it == open_sessions_.end()) {
    promise->resolve();
    return;
  }

  // 5.1. Let cdm be the CDM instance represented by session's cdm instance
  //      value.
  // 5.2. Use cdm to close the session associated with session.
  open_sessions_.erase(it);
  DeleteKeysForSession(session_id);

  // 5.3. Queue a task to run the following steps:
  // 5.3.1. Run the Session Closed algorithm on the session.
  session_closed_cb_.Run(session_id);
  // 5.3.2. Resolve promise.
  promise->resolve();
}

// Runs the parallel steps from https://w3c.github.io/encrypted-media/#remove.
void AesDecryptor::RemoveSession(const std::string& session_id,
                                 std::unique_ptr<SimpleCdmPromise> promise) {
  auto it = open_sessions_.find(session_id);
  if (it == open_sessions_.end()) {
    // Session doesn't exist. Since this should only be called if the session
    // existed at one time, this must mean the session has been closed.
    promise->reject(CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "The session is already closed.");
    return;
  }

  // Create the list of all existing keys for this session. They will be
  // removed, so set the status to "released".
  CdmKeysInfo keys_info =
      GenerateKeysInfoList(session_id, CdmKeyInformation::RELEASED);

  // 4.1. Let cdm be the CDM instance represented by session's cdm instance
  //      value.
  // 4.2 Let message be null.
  // 4.3 Let message type be null.
  // 4.4 Use the cdm to execute the following steps:
  // 4.4.1.1 Destroy the license(s) and/or key(s) associated with the session.
  DeleteKeysForSession(session_id);

  // 4.4.1.2 Follow the steps for the value of this object's session type
  //         from the following list:
  //           "temporary"
  //              Continue with the following steps.
  //           "persistent-license"
  //              Let message be a message containing or reflecting the record
  //              of license destruction.
  std::vector<uint8_t> message;
  if (it->second != CdmSessionType::kTemporary) {
    // The license release message is specified in the spec:
    // https://w3c.github.io/encrypted-media/#clear-key-release-format.
    KeyIdList key_ids;
    key_ids.reserve(keys_info.size());
    for (const auto& key_info : keys_info)
      key_ids.push_back(key_info->key_id);
    CreateKeyIdsInitData(key_ids, &message);
  }

  // 4.5. Queue a task to run the following steps:
  // 4.5.1 Run the Update Key Statuses algorithm on the session, providing
  //       all key ID(s) in the session along with the "released"
  //       MediaKeyStatus value for each.
  session_keys_change_cb_.Run(session_id, false, std::move(keys_info));

  // 4.5.2 Run the Update Expiration algorithm on the session, providing NaN.
  session_expiration_update_cb_.Run(session_id, base::Time());

  // 4.5.3 If any of the preceding steps failed, reject promise with a new
  //       DOMException whose name is the appropriate error name.
  // 4.5.4 Let message type be "license-release".
  // 4.5.5 If message is not null, run the Queue a "message" Event algorithm
  //       on the session, providing message type and message.
  if (!message.empty())
    session_message_cb_.Run(session_id, CdmMessageType::LICENSE_RELEASE,
                            message);

  // 4.5.6. Resolve promise.
  promise->resolve();
}

CdmContext* AesDecryptor::GetCdmContext() {
  return this;
}

std::unique_ptr<CallbackRegistration> AesDecryptor::RegisterEventCB(
    EventCB event_cb) {
  NOTIMPLEMENTED();
  return nullptr;
}

Decryptor* AesDecryptor::GetDecryptor() {
  return this;
}

int AesDecryptor::GetCdmId() const {
  return kInvalidCdmId;
}

void AesDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                    const NewKeyCB& new_key_cb) {
  base::AutoLock auto_lock(new_key_cb_lock_);

  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = new_key_cb;
      break;
    case kVideo:
      new_video_key_cb_ = new_key_cb;
      break;
    default:
      NOTREACHED();
  }
}

void AesDecryptor::Decrypt(StreamType stream_type,
                           scoped_refptr<DecoderBuffer> encrypted,
                           const DecryptCB& decrypt_cb) {
  DVLOG(3) << __func__ << ": " << encrypted->AsHumanReadableString();

  if (!encrypted->decrypt_config()) {
    // If there is no DecryptConfig, then the data is unencrypted so return it
    // immediately.
    decrypt_cb.Run(kSuccess, encrypted);
    return;
  }

  const std::string& key_id = encrypted->decrypt_config()->key_id();
  base::AutoLock auto_lock(key_map_lock_);
  DecryptionKey* key = GetKey_Locked(key_id);
  if (!key) {
    DVLOG(1) << "Could not find a matching key for the given key ID.";
    decrypt_cb.Run(kNoKey, nullptr);
    return;
  }

  scoped_refptr<DecoderBuffer> decrypted =
      DecryptData(*encrypted.get(), *key->decryption_key());
  if (!decrypted) {
    DVLOG(1) << "Decryption failed.";
    decrypt_cb.Run(kError, nullptr);
    return;
  }

  DCHECK_EQ(decrypted->timestamp(), encrypted->timestamp());
  DCHECK_EQ(decrypted->duration(), encrypted->duration());
  decrypt_cb.Run(kSuccess, std::move(decrypted));
}

void AesDecryptor::CancelDecrypt(StreamType stream_type) {
  // Decrypt() calls the DecryptCB synchronously so there's nothing to cancel.
}

void AesDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support audio decoding.
  init_cb.Run(false);
}

void AesDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support video decoding.
  init_cb.Run(false);
}

void AesDecryptor::DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                                         const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support audio decoding";
}

void AesDecryptor::DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                                         const VideoDecodeCB& video_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support video decoding";
}

void AesDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "AesDecryptor does not support audio/video decoding";
}

void AesDecryptor::DeinitializeDecoder(StreamType stream_type) {
  // AesDecryptor does not support audio/video decoding, but since this can be
  // called any time after InitializeAudioDecoder/InitializeVideoDecoder,
  // nothing to be done here.
}

bool AesDecryptor::CanAlwaysDecrypt() {
  return true;
}

bool AesDecryptor::CreateSession(const std::string& session_id,
                                 CdmSessionType session_type) {
  auto it = open_sessions_.find(session_id);
  if (it != open_sessions_.end())
    return false;

  auto result = open_sessions_.emplace(session_id, session_type);
  return result.second;
}

std::string AesDecryptor::GetSessionStateAsJWK(const std::string& session_id) {
  // Create the list of all available keys for this session.
  KeyIdAndKeyPairs keys;
  {
    base::AutoLock auto_lock(key_map_lock_);
    for (const auto& item : key_map_) {
      if (item.second->Contains(session_id)) {
        std::string key_id = item.first;
        // |key| is the value used to create the decryption key.
        std::string key = item.second->LatestDecryptionKey()->secret();
        keys.push_back(std::make_pair(key_id, key));
      }
    }
  }
  return GenerateJWKSet(keys, CdmSessionType::kPersistentLicense);
}

bool AesDecryptor::AddDecryptionKey(const std::string& session_id,
                                    const std::string& key_id,
                                    const std::string& key_string) {
  std::unique_ptr<DecryptionKey> decryption_key(new DecryptionKey(key_string));
  if (!decryption_key->Init()) {
    DVLOG(1) << "Could not initialize decryption key.";
    return false;
  }

  base::AutoLock auto_lock(key_map_lock_);
  auto key_id_entry = key_map_.find(key_id);
  if (key_id_entry != key_map_.end()) {
    key_id_entry->second->Insert(session_id, std::move(decryption_key));
    return true;
  }

  // |key_id| not found, so need to create new entry.
  std::unique_ptr<SessionIdDecryptionKeyMap> inner_map(
      new SessionIdDecryptionKeyMap());
  inner_map->Insert(session_id, std::move(decryption_key));
  key_map_[key_id] = std::move(inner_map);
  return true;
}

AesDecryptor::DecryptionKey* AesDecryptor::GetKey_Locked(
    const std::string& key_id) const {
  key_map_lock_.AssertAcquired();
  auto key_id_found = key_map_.find(key_id);
  if (key_id_found == key_map_.end())
    return NULL;

  // Return the key from the "latest" session_id entry.
  return key_id_found->second->LatestDecryptionKey();
}

bool AesDecryptor::HasKey(const std::string& session_id,
                          const std::string& key_id) {
  base::AutoLock auto_lock(key_map_lock_);
  KeyIdToSessionKeysMap::const_iterator key_id_found = key_map_.find(key_id);
  if (key_id_found == key_map_.end())
    return false;

  return key_id_found->second->Contains(session_id);
}

void AesDecryptor::DeleteKeysForSession(const std::string& session_id) {
  base::AutoLock auto_lock(key_map_lock_);

  // Remove all keys associated with |session_id|. Since the data is
  // optimized for access in GetKey_Locked(), we need to look at each entry in
  // |key_map_|.
  auto it = key_map_.begin();
  while (it != key_map_.end()) {
    it->second->Erase(session_id);
    if (it->second->Empty()) {
      // Need to get rid of the entry for this key_id. This will mess up the
      // iterator, so we need to increment it first.
      auto current = it;
      ++it;
      key_map_.erase(current);
    } else {
      ++it;
    }
  }
}

CdmKeysInfo AesDecryptor::GenerateKeysInfoList(
    const std::string& session_id,
    CdmKeyInformation::KeyStatus status) {
  // Create the list of all available keys for this session.
  CdmKeysInfo keys_info;
  {
    base::AutoLock auto_lock(key_map_lock_);
    for (const auto& item : key_map_) {
      if (item.second->Contains(session_id)) {
        keys_info.push_back(
            std::make_unique<CdmKeyInformation>(item.first, status, 0));
      }
    }
  }
  return keys_info;
}

AesDecryptor::DecryptionKey::DecryptionKey(const std::string& secret)
    : secret_(secret) {}

AesDecryptor::DecryptionKey::~DecryptionKey() = default;

bool AesDecryptor::DecryptionKey::Init() {
  CHECK(!secret_.empty());
  decryption_key_ =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, secret_);
  if (!decryption_key_)
    return false;
  return true;
}

}  // namespace media
