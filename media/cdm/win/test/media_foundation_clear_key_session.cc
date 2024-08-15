// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/win/test/media_foundation_clear_key_session.h"

#include <mfapi.h>
#include <mferror.h>
#include <wrl/client.h>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"

namespace media {

using Microsoft::WRL::ComPtr;

namespace {

MF_MEDIAKEY_STATUS ToMFKeyStatus(media::CdmKeyInformation::KeyStatus status) {
  switch (status) {
    case media::CdmKeyInformation::KeyStatus::USABLE:
      return MF_MEDIAKEY_STATUS_USABLE;
    case media::CdmKeyInformation::KeyStatus::EXPIRED:
      return MF_MEDIAKEY_STATUS_EXPIRED;
    case media::CdmKeyInformation::KeyStatus::OUTPUT_DOWNSCALED:
      return MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED;
    case media::CdmKeyInformation::KeyStatus::KEY_STATUS_PENDING:
      return MF_MEDIAKEY_STATUS_STATUS_PENDING;
    case media::CdmKeyInformation::KeyStatus::INTERNAL_ERROR:
      return MF_MEDIAKEY_STATUS_INTERNAL_ERROR;
    case media::CdmKeyInformation::KeyStatus::RELEASED:
      return MF_MEDIAKEY_STATUS_RELEASED;
    case media::CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED:
      return MF_MEDIAKEY_STATUS_OUTPUT_RESTRICTED;
  }
}

media::CdmSessionType ToCdmSessionType(MF_MEDIAKEYSESSION_TYPE session_type) {
  switch (session_type) {
    case MF_MEDIAKEYSESSION_TYPE_TEMPORARY:
      return media::CdmSessionType::kTemporary;
    case MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE:
      return media::CdmSessionType::kPersistentLicense;
    case MF_MEDIAKEYSESSION_TYPE_PERSISTENT_RELEASE_MESSAGE:
    case MF_MEDIAKEYSESSION_TYPE_PERSISTENT_USAGE_RECORD:
      NOTREACHED();
  }
}

MF_MEDIAKEYSESSION_MESSAGETYPE ToMFMessageType(
    media::CdmMessageType message_type) {
  switch (message_type) {
    case media::CdmMessageType::LICENSE_REQUEST:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST;
    case media::CdmMessageType::LICENSE_RENEWAL:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RENEWAL;
    case media::CdmMessageType::LICENSE_RELEASE:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE;
    case media::CdmMessageType::INDIVIDUALIZATION_REQUEST:
      return MF_MEDIAKEYSESSION_MESSAGETYPE_INDIVIDUALIZATION_REQUEST;
  }
}

enum class PromiseState { kPending, kResolved, kRejected };

class MediaFoundationSimpleCdmPromise : public SimpleCdmPromise {
 public:
  explicit MediaFoundationSimpleCdmPromise(PromiseState* promise_state) {
    DVLOG_FUNC(1);
    CHECK(promise_state);

    promise_state_ = promise_state;
    *promise_state_ = PromiseState::kPending;
  }

  MediaFoundationSimpleCdmPromise(const MediaFoundationSimpleCdmPromise&) =
      delete;
  MediaFoundationSimpleCdmPromise& operator=(
      const MediaFoundationSimpleCdmPromise&) = delete;

  ~MediaFoundationSimpleCdmPromise() override { DVLOG_FUNC(1); }

  void resolve() override {
    DVLOG_FUNC(1);

    *promise_state_ = PromiseState::kResolved;
    MarkPromiseSettled();
  }

  void reject(CdmPromise::Exception, uint32_t, const std::string&) override {
    DVLOG_FUNC(1);

    *promise_state_ = PromiseState::kRejected;
    MarkPromiseSettled();
  }

 private:
  raw_ptr<PromiseState> promise_state_ = nullptr;
};

class MediaFoundationCdmSessionPromise : public NewSessionCdmPromise {
 public:
  MediaFoundationCdmSessionPromise(PromiseState* promise_state,
                                   SessionIdCB session_created_cb) {
    DVLOG_FUNC(1);
    CHECK(promise_state);
    CHECK(session_created_cb);

    promise_state_ = promise_state;
    *promise_state_ = PromiseState::kPending;
    session_created_cb_ = std::move(session_created_cb);
  }

  MediaFoundationCdmSessionPromise(const MediaFoundationCdmSessionPromise&) =
      delete;
  MediaFoundationCdmSessionPromise& operator=(
      const MediaFoundationCdmSessionPromise&) = delete;

  ~MediaFoundationCdmSessionPromise() override { DVLOG_FUNC(1); }

  void resolve(const std::string& new_session_id) override {
    DVLOG_FUNC(1) << "new_session_id=" << new_session_id;

    *promise_state_ = PromiseState::kResolved;

    // Notify new session id back to CDM first before AesDecryptor raises
    // SessionMessage callback.
    std::move(session_created_cb_).Run(new_session_id);

    MarkPromiseSettled();
  }

  void reject(CdmPromise::Exception, uint32_t, const std::string&) override {
    DVLOG_FUNC(1);

    *promise_state_ = PromiseState::kRejected;
    MarkPromiseSettled();
  }

 private:
  raw_ptr<PromiseState> promise_state_ = nullptr;
  SessionIdCB session_created_cb_;
};

}  // namespace

MediaFoundationClearKeySession::MediaFoundationClearKeySession() {
  DVLOG_FUNC(1);
}

MediaFoundationClearKeySession::~MediaFoundationClearKeySession() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeySession::RuntimeClassInitialize(
    _In_ MF_MEDIAKEYSESSION_TYPE session_type,
    _In_ IMFContentDecryptionModuleSessionCallbacks* callbacks,
    _In_ scoped_refptr<AesDecryptor> aes_decryptor,
    _In_ SessionIdCreatedCB session_id_created_cb,
    _In_ SessionIdCB session_id_removed_cb) {
  DVLOG_FUNC(1);
  CHECK(session_type == MF_MEDIAKEYSESSION_TYPE_TEMPORARY);
  CHECK(callbacks);
  CHECK(aes_decryptor);
  CHECK(session_id_created_cb);
  CHECK(session_id_removed_cb);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  session_type_ = session_type;
  callbacks_ = callbacks;
  aes_decryptor_ = std::move(aes_decryptor);
  session_id_created_cb_ = std::move(session_id_created_cb);
  session_id_removed_cb_ = std::move(session_id_removed_cb);

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeySession::Update(
    _In_reads_bytes_(response_size) const BYTE* response,
    _In_ DWORD response_size) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (session_id_.empty()) {
    return MF_INVALID_STATE_ERR;
  }

  if (!response || response_size == 0 || response[0] == 0) {
    return MF_TYPE_ERR;
  }

  PromiseState promise_state = PromiseState::kPending;
  aes_decryptor_->UpdateSession(
      session_id_, std::vector<uint8_t>(response, response + response_size),
      std::make_unique<MediaFoundationSimpleCdmPromise>(&promise_state));

  CHECK(promise_state != PromiseState::kPending);
  return promise_state == PromiseState::kResolved ? S_OK : E_FAIL;
}

STDMETHODIMP MediaFoundationClearKeySession::Close() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  PromiseState promise_state = PromiseState::kPending;
  aes_decryptor_->CloseSession(
      session_id_,
      std::make_unique<MediaFoundationSimpleCdmPromise>(&promise_state));
  CHECK(promise_state != PromiseState::kPending);

  if (session_id_removed_cb_) {
    std::move(session_id_removed_cb_).Run(session_id_);
  }

  session_id_.clear();

  return promise_state == PromiseState::kResolved ? S_OK : E_FAIL;
}

STDMETHODIMP MediaFoundationClearKeySession::GetSessionId(
    _COM_Outptr_ LPWSTR* id) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (session_id_.length() == 0) {
    RETURN_IF_FAILED(CopyCoTaskMemWideString(L"", id));
    return S_OK;
  }

  RETURN_IF_FAILED(
      CopyCoTaskMemWideString(base::ASCIIToWide(session_id_).c_str(), id));

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeySession::GetKeyStatuses(
    _Outptr_result_buffer_(*key_statuses_size) MFMediaKeyStatus** key_statuses,
    _Out_ UINT* key_statuses_count) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  *key_statuses = nullptr;
  *key_statuses_count = 0;

  const auto key_status_count = keys_info_.size();
  if (session_id_.empty() || key_status_count == 0) {
    // Return an empty sequence.
    return S_OK;
  }

  MFMediaKeyStatus* key_status_array = nullptr;
  key_status_array = static_cast<MFMediaKeyStatus*>(
      CoTaskMemAlloc(key_status_count * sizeof(MFMediaKeyStatus)));
  if (key_status_array == nullptr) {
    return E_OUTOFMEMORY;
  }
  ZeroMemory(key_status_array, key_status_count * sizeof(MFMediaKeyStatus));

  // Special key ID to crash the CDM. The key ID must match the key ID used
  // for crash testing in media/test/data/media_foundation_fallback.html
  const std::vector<uint8_t> kCrashKeyId =
      ByteArrayFromGUID(GetGUIDFromString("crash-crashcrash"));

  for (UINT i = 0; i < key_status_count; ++i) {
    key_status_array[i].cbKeyId = keys_info_[i]->key_id.size();
    key_status_array[i].pbKeyId = static_cast<BYTE*>(
        CoTaskMemAlloc(keys_info_[i]->key_id.size() * sizeof(uint8_t)));
    if (key_status_array[i].pbKeyId == nullptr) {
      return E_OUTOFMEMORY;
    }

    if (keys_info_[i]->key_id == kCrashKeyId) {
      CHECK(false) << "Crash on special crash key ID.";
    }

    key_status_array[i].eMediaKeyStatus = ToMFKeyStatus(keys_info_[i]->status);
    memcpy(key_status_array[i].pbKeyId, keys_info_[i]->key_id.data(),
           keys_info_[i]->key_id.size());
  }

  *key_statuses = key_status_array;
  *key_statuses_count = key_status_count;

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeySession::Load(_In_ LPCWSTR session_id,
                                                  _Out_ BOOL* loaded) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // LoadSession() is not supported since only temporary sessions are supported
  // for ClearKey.
  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP MediaFoundationClearKeySession::GenerateRequest(
    _In_ LPCWSTR init_data_type,
    _In_reads_bytes_(init_data_size) const BYTE* init_data,
    _In_ DWORD init_data_size) {
  DVLOG_FUNC(1) << ", init_data_size=" << init_data_size;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!session_id_.empty()) {
    return MF_INVALID_STATE_ERR;
  }

  if (!init_data || init_data_size == 0) {
    return MF_TYPE_ERR;
  }

  EmeInitDataType eme_init_data_type = EmeInitDataType::UNKNOWN;

  if (wcscmp(init_data_type, L"cenc") == 0) {
    eme_init_data_type = EmeInitDataType::CENC;
    DVLOG_FUNC(3) << "eme_init_data_type=CENC";
  } else if (wcscmp(init_data_type, L"webm") == 0) {
    eme_init_data_type = EmeInitDataType::WEBM;
    DVLOG_FUNC(3) << "eme_init_data_type=WEBM";
  } else if (wcscmp(init_data_type, L"keyids") == 0) {
    eme_init_data_type = EmeInitDataType::KEYIDS;
    DVLOG_FUNC(3) << "eme_init_data_type=KEYIDS";
  } else {
    DLOG(ERROR) << __func__
                << ": Unsupported init_data_type=" << init_data_type;
    return MF_NOT_SUPPORTED_ERR;
  }

  PromiseState promise_state = PromiseState::kPending;
  media::CdmSessionType cdm_session_type = ToCdmSessionType(session_type_);
  aes_decryptor_->CreateSessionAndGenerateRequest(
      cdm_session_type, eme_init_data_type,
      std::vector<uint8_t>(init_data, init_data + init_data_size),
      std::make_unique<MediaFoundationCdmSessionPromise>(
          &promise_state,
          base::BindOnce(&MediaFoundationClearKeySession::OnSessionCreated,
                         weak_factory_.GetWeakPtr())));

  CHECK(promise_state != PromiseState::kPending);
  return promise_state == PromiseState::kResolved ? S_OK : E_FAIL;
}

STDMETHODIMP MediaFoundationClearKeySession::GetExpiration(
    _Out_ double* expiration) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Never expires for testing.
  *expiration = 0.0;

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeySession::Remove() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  PromiseState promise_state = PromiseState::kPending;
  aes_decryptor_->RemoveSession(
      session_id_,
      std::make_unique<MediaFoundationSimpleCdmPromise>(&promise_state));
  CHECK(promise_state != PromiseState::kPending);
  return promise_state == PromiseState::kResolved ? S_OK : E_FAIL;
}

void MediaFoundationClearKeySession::OnSessionCreated(
    const std::string& session_id) {
  DVLOG_FUNC(1) << "session_id=" << session_id;
  CHECK(session_id_created_cb_);

  session_id_ = session_id;
  std::move(session_id_created_cb_).Run(session_id, this);
}

void MediaFoundationClearKeySession::OnSessionMessage(
    const std::string& session_id,
    CdmMessageType message_type,
    const std::vector<uint8_t>& message) {
  DVLOG_FUNC(1) << "session_id=" << session_id
                << ", message_type=" << static_cast<int>(message_type);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto mf_message_type = ToMFMessageType(message_type);
  callbacks_->KeyMessage(mf_message_type, message.data(), message.size(),
                         nullptr);
}

void MediaFoundationClearKeySession::OnSessionClosed(
    const std::string& session_id,
    CdmSessionClosedReason reason) {
  DVLOG_FUNC(1) << "session_id=" << session_id
                << ", reason=" << static_cast<int>(reason);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void MediaFoundationClearKeySession::OnSessionKeysChange(
    const std::string& session_id,
    bool has_additional_usable_key,
    CdmKeysInfo keys_info) {
  DVLOG_FUNC(1) << "session_id=" << session_id
                << ", has_additional_usable_key=" << has_additional_usable_key;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (size_t i = 0; i < keys_info.size(); ++i) {
    DVLOG_FUNC(3) << "key_info[" << i << "]=" << *keys_info[i];
  }

  keys_info_ = std::move(keys_info);

  callbacks_->KeyStatusChanged();
}

}  // namespace media
