// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/win/media_foundation_cdm_session.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/win/scoped_co_mem.h"
#include "media/base/cdm_key_information.h"
#include "media/base/win/mf_helpers.h"

namespace media {

namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

MF_MEDIAKEYSESSION_TYPE ToMFSessionType(CdmSessionType session_type) {
  switch (session_type) {
    case CdmSessionType::kTemporary:
      return MF_MEDIAKEYSESSION_TYPE_TEMPORARY;
    case CdmSessionType::kPersistentLicense:
      return MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE;
  }
}

// The strings are defined in https://www.w3.org/TR/eme-initdata-registry/
LPCWSTR InitDataTypeToString(EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case EmeInitDataType::UNKNOWN:
      return L"unknown";
    case EmeInitDataType::WEBM:
      return L"webm";
    case EmeInitDataType::CENC:
      return L"cenc";
    case EmeInitDataType::KEYIDS:
      return L"keyids";
  }
}

CdmMessageType ToCdmMessageType(MF_MEDIAKEYSESSION_MESSAGETYPE message_type) {
  switch (message_type) {
    case MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST:
      return CdmMessageType::LICENSE_REQUEST;
    case MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RENEWAL:
      return CdmMessageType::LICENSE_RENEWAL;
    case MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE:
      return CdmMessageType::LICENSE_RELEASE;
    case MF_MEDIAKEYSESSION_MESSAGETYPE_INDIVIDUALIZATION_REQUEST:
      return CdmMessageType::INDIVIDUALIZATION_REQUEST;
  }
}

CdmKeyInformation::KeyStatus ToCdmKeyStatus(MF_MEDIAKEY_STATUS status) {
  switch (status) {
    case MF_MEDIAKEY_STATUS_USABLE:
      return CdmKeyInformation::KeyStatus::USABLE;
    case MF_MEDIAKEY_STATUS_EXPIRED:
      return CdmKeyInformation::KeyStatus::EXPIRED;
    case MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED:
      return CdmKeyInformation::KeyStatus::OUTPUT_DOWNSCALED;
    // This is for legacy use and should not happen in normal cases. Map it to
    // internal error in case it happens.
    case MF_MEDIAKEY_STATUS_OUTPUT_NOT_ALLOWED:
      return CdmKeyInformation::KeyStatus::INTERNAL_ERROR;
    case MF_MEDIAKEY_STATUS_STATUS_PENDING:
      return CdmKeyInformation::KeyStatus::KEY_STATUS_PENDING;
    case MF_MEDIAKEY_STATUS_INTERNAL_ERROR:
      return CdmKeyInformation::KeyStatus::INTERNAL_ERROR;
    case MF_MEDIAKEY_STATUS_RELEASED:
      return CdmKeyInformation::KeyStatus::RELEASED;
    case MF_MEDIAKEY_STATUS_OUTPUT_RESTRICTED:
      return CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED;
  }
}

CdmKeysInfo ToCdmKeysInfo(const MFMediaKeyStatus* key_statuses, int count) {
  CdmKeysInfo keys_info;
  keys_info.reserve(count);
  for (int i = 0; i < count; ++i) {
    const auto& key_status = key_statuses[i];

    if (key_status.cbKeyId != sizeof(GUID)) {
      DLOG(ERROR) << __func__ << ": Key ID with unsupported size ignored";
      continue;
    }

    GUID* key_id_guid = reinterpret_cast<GUID*>(key_status.pbKeyId);
    keys_info.push_back(std::make_unique<CdmKeyInformation>(
        ByteArrayFromGUID(*key_id_guid),
        ToCdmKeyStatus(key_status.eMediaKeyStatus),
        /*system_code=*/0));
  }
  return keys_info;
}

class SessionCallbacks final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                          IMFContentDecryptionModuleSessionCallbacks> {
 public:
  SessionCallbacks() { DVLOG_FUNC(1); }
  SessionCallbacks(const SessionCallbacks&) = delete;
  SessionCallbacks& operator=(const SessionCallbacks&) = delete;
  ~SessionCallbacks() override { DVLOG_FUNC(1); }

  using MessageCB =
      base::RepeatingCallback<void(CdmMessageType message_type,
                                   const std::vector<uint8_t>& message)>;
  using KeysChangeCB = base::RepeatingClosure;

  HRESULT RuntimeClassInitialize(MessageCB message_cb,
                                 KeysChangeCB keys_change_cb) {
    message_cb_ = std::move(message_cb);
    keys_change_cb_ = std::move(keys_change_cb);
    return S_OK;
  }

  // IMFContentDecryptionModuleSessionCallbacks implementation

  STDMETHODIMP KeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE message_type,
                          const BYTE* message,
                          DWORD message_size,
                          LPCWSTR destination_url) final {
    DVLOG_FUNC(2) << ": message size=" << message_size;
    message_cb_.Run(ToCdmMessageType(message_type),
                    std::vector<uint8_t>(message, message + message_size));
    return S_OK;
  }

  STDMETHODIMP KeyStatusChanged() final {
    DVLOG_FUNC(2);
    keys_change_cb_.Run();
    return S_OK;
  }

 private:
  MessageCB message_cb_;
  KeysChangeCB keys_change_cb_;
};

}  // namespace

MediaFoundationCdmSession::MediaFoundationCdmSession(
    const std::string& uma_prefix,
    const SessionMessageCB& session_message_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : uma_prefix_(uma_prefix),
      session_message_cb_(session_message_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  DVLOG_FUNC(1);
}

MediaFoundationCdmSession::~MediaFoundationCdmSession() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationCdmSession::Initialize(
    IMFContentDecryptionModule* mf_cdm,
    CdmSessionType session_type) {
  DVLOG_FUNC(1);

  ComPtr<SessionCallbacks> session_callbacks;
  auto weak_this = weak_factory_.GetWeakPtr();

  // Use base::BindPostTaskToCurrentDefault() because the callbacks can be fired
  // on different threads by |mf_cdm_session_|.
  RETURN_IF_FAILED(MakeAndInitialize<SessionCallbacks>(
      &session_callbacks,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationCdmSession::OnSessionMessage, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationCdmSession::OnSessionKeysChange, weak_this))));

  // |mf_cdm_session_| holds a ref count to |session_callbacks|.
  RETURN_IF_FAILED(mf_cdm->CreateSession(ToMFSessionType(session_type),
                                         session_callbacks.Get(),
                                         &mf_cdm_session_));

  return S_OK;
}

HRESULT MediaFoundationCdmSession::GenerateRequest(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    SessionIdCB session_id_cb) {
  DVLOG_FUNC(1);
  DCHECK(session_id_.empty() && !session_id_cb_);

  session_id_cb_ = std::move(session_id_cb);

  RETURN_IF_FAILED(WithUmaReported(
      mf_cdm_session_->GenerateRequest(
          InitDataTypeToString(init_data_type), init_data.data(),
          base::checked_cast<DWORD>(init_data.size())),
      "GenerateRequest"));
  return S_OK;
}

HRESULT MediaFoundationCdmSession::Load(const std::string& session_id) {
  DVLOG_FUNC(1);
  RETURN_IF_FAILED(WithUmaReported(E_NOTIMPL, "LoadSession"));
  return S_OK;
}

HRESULT
MediaFoundationCdmSession::Update(const std::vector<uint8_t>& response) {
  DVLOG_FUNC(1);
  RETURN_IF_FAILED(WithUmaReported(
      mf_cdm_session_->Update(reinterpret_cast<const BYTE*>(response.data()),
                              base::checked_cast<DWORD>(response.size())),
      "UpdateSession"));
  RETURN_IF_FAILED(UpdateExpirationIfNeeded());
  return S_OK;
}

HRESULT MediaFoundationCdmSession::Close() {
  DVLOG_FUNC(1);
  RETURN_IF_FAILED(WithUmaReported(mf_cdm_session_->Close(), "CloseSession"));
  return S_OK;
}

HRESULT MediaFoundationCdmSession::Remove() {
  DVLOG_FUNC(1);
  RETURN_IF_FAILED(WithUmaReported(mf_cdm_session_->Remove(), "RemoveSession"));
  RETURN_IF_FAILED(UpdateExpirationIfNeeded());
  return S_OK;
}

HRESULT MediaFoundationCdmSession::WithUmaReported(HRESULT hr,
                                                   const std::string& api) {
  base::UmaHistogramSparse(uma_prefix_ + api, hr);
  return hr;
}

void MediaFoundationCdmSession::OnSessionMessage(
    CdmMessageType message_type,
    const std::vector<uint8_t>& message) {
  DVLOG_FUNC(2);

  if (session_id_.empty() && !session_id_cb_) {
    DLOG(ERROR) << "Unexpected session message";
    return;
  }

  // If |session_id_| has not been set, set it now.
  if (session_id_.empty() && !SetSessionId())
    return;

  DCHECK(!session_id_.empty());
  session_message_cb_.Run(session_id_, message_type, message);
}

void MediaFoundationCdmSession::OnSessionKeysChange() {
  DVLOG_FUNC(2);

  if (session_id_.empty()) {
    DLOG(ERROR) << "Unexpected session keys change ignored";
    return;
  }

  base::win::ScopedCoMem<MFMediaKeyStatus> key_statuses;

  UINT count = 0;
  if (FAILED(mf_cdm_session_->GetKeyStatuses(&key_statuses, &count))) {
    DLOG(ERROR) << __func__ << ": Failed to get key statuses";
    return;
  }

  // TODO(xhwang): Investigate whether we need to set |has_new_usable_key|.
  session_keys_change_cb_.Run(session_id_, /*has_new_usable_key=*/true,
                              ToCdmKeysInfo(key_statuses.get(), count));

  // ScopedCoMem<MFMediaKeyStatus> only releases memory for |key_statuses|. We
  // need to manually release memory for |pbKeyId| here.
  for (UINT i = 0; i < count; ++i) {
    const auto& key_status = key_statuses[i];
    if (key_status.pbKeyId)
      CoTaskMemFree(key_status.pbKeyId);
  }
}

bool MediaFoundationCdmSession::SetSessionId() {
  DCHECK(session_id_.empty() && session_id_cb_);

  base::win::ScopedCoMem<wchar_t> session_id;
  HRESULT hr = mf_cdm_session_->GetSessionId(&session_id);
  if (FAILED(hr) || !session_id) {
    bool success = std::move(session_id_cb_).Run("");
    DCHECK(!success) << "Empty session ID should not be accepted";
    return false;
  }

  auto session_id_str = base::WideToUTF8(session_id.get());
  if (session_id_str.empty()) {
    bool success = std::move(session_id_cb_).Run("");
    DCHECK(!success) << "Empty session ID should not be accepted";
    return false;
  }

  bool success = std::move(session_id_cb_).Run(session_id_str);
  if (!success) {
    DLOG(ERROR) << "Session ID " << session_id_str << " rejected";
    return false;
  }

  DVLOG_FUNC(1) << "session_id_=" << session_id_str;
  session_id_ = session_id_str;
  return true;
}

HRESULT MediaFoundationCdmSession::UpdateExpirationIfNeeded() {
  DCHECK(!session_id_.empty());

  // Media Foundation CDM follows the EME spec where Time generally represents
  // an instant in time with millisecond accuracy.
  double new_expiration_ms = 0.0;
  RETURN_IF_FAILED(mf_cdm_session_->GetExpiration(&new_expiration_ms));
  auto new_expiration =
      base::Time::FromMillisecondsSinceUnixEpoch(new_expiration_ms);

  if (new_expiration == expiration_)
    return S_OK;

  DVLOG(2) << "New session expiration: " << new_expiration;
  expiration_ = new_expiration;
  session_expiration_update_cb_.Run(session_id_, expiration_);
  return S_OK;
}

}  // namespace media
