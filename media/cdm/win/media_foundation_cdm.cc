// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm.h"

#include <stdlib.h>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/win_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_promise.h"
#include "media/base/win/mf_cdm_proxy.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/media_foundation_cdm_session.h"

namespace media {

namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Exception = CdmPromise::Exception;

// GUID is little endian. The byte array in network order is big endian.
std::vector<uint8_t> ByteArrayFromGUID(REFGUID guid) {
  std::vector<uint8_t> byte_array(sizeof(GUID));
  GUID* reversed_guid = reinterpret_cast<GUID*>(byte_array.data());
  *reversed_guid = guid;
  reversed_guid->Data1 = _byteswap_ulong(guid.Data1);
  reversed_guid->Data2 = _byteswap_ushort(guid.Data2);
  reversed_guid->Data3 = _byteswap_ushort(guid.Data3);
  // Data4 is already a byte array so no need to byte swap.
  return byte_array;
}

HRESULT CreatePolicySetEvent(ComPtr<IMFMediaEvent>& policy_set_event) {
  base::win::ScopedPropVariant policy_set_prop;
  PROPVARIANT* var_to_set = policy_set_prop.Receive();
  var_to_set->vt = VT_UI4;
  var_to_set->ulVal = 0;
  RETURN_IF_FAILED(MFCreateMediaEvent(
      MEPolicySet, GUID_NULL, S_OK, policy_set_prop.ptr(), &policy_set_event));
  return S_OK;
}

// Notifies the Decryptor about the last key ID so the decryptor can prefetch
// the corresponding key to reduce start-to-play time when resuming playback.
// This is done by sending a MEContentProtectionMetadata event.
HRESULT RefreshDecryptor(IMFTransform* decryptor,
                         const GUID& protection_system_id,
                         const GUID& last_key_id) {
  // The MFT_MESSAGE_NOTIFY_START_OF_STREAM message is usually sent by the MF
  // pipeline when starting playback. Here we send it out-of-band as it is a
  // pre-requisite for getting the decryptor to process the
  // MEContentProtectionMetadata event.
  RETURN_IF_FAILED(
      decryptor->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

  // After receiving a MEContentProtectionMetadata event, the Decryptor
  // requires that it is notified of a MEPolicySet event to continue decryption.
  ComPtr<IMFMediaEvent> policy_set_event;
  RETURN_IF_FAILED(CreatePolicySetEvent(policy_set_event));
  RETURN_IF_FAILED(decryptor->ProcessMessage(
      MFT_MESSAGE_NOTIFY_EVENT,
      reinterpret_cast<ULONG_PTR>(policy_set_event.Get())));

  // Prepare the MEContentProtectionMetadata event.
  ComPtr<IMFMediaEvent> key_rotation_event;
  RETURN_IF_FAILED(MFCreateMediaEvent(MEContentProtectionMetadata, GUID_NULL,
                                      S_OK, nullptr, &key_rotation_event));

  // MF_EVENT_STREAM_METADATA_SYSTEMID expects the system ID (GUID) to be in
  // little endian order. So no need to call `ByteArrayFromGUID()`.
  RETURN_IF_FAILED(key_rotation_event->SetBlob(
      MF_EVENT_STREAM_METADATA_SYSTEMID,
      reinterpret_cast<const uint8_t*>(&protection_system_id), sizeof(GUID)));

  std::vector<uint8_t> last_key_id_byte_array = ByteArrayFromGUID(last_key_id);
  RETURN_IF_FAILED(key_rotation_event->SetBlob(
      MF_EVENT_STREAM_METADATA_CONTENT_KEYIDS, last_key_id_byte_array.data(),
      last_key_id_byte_array.size()));

  // The `dwInputStreamID` refers to a local stream ID of the Decryptor. Since
  // Decryptors typically only support a single stream, always pass 0 here.
  RETURN_IF_FAILED(
      decryptor->ProcessEvent(/*dwInputStreamID=*/0, key_rotation_event.Get()));
  return S_OK;
}

class CdmProxyImpl
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFCdmProxy> {
 public:
  explicit CdmProxyImpl(ComPtr<IMFContentDecryptionModule> mf_cdm)
      : mf_cdm_(mf_cdm) {}
  ~CdmProxyImpl() override = default;

  // IMFCdmProxy implementation

  STDMETHODIMP GetPMPServer(REFIID riid, LPVOID* object_result) override {
    DVLOG_FUNC(1);
    ComPtr<IMFGetService> cdm_services;
    RETURN_IF_FAILED(mf_cdm_.As(&cdm_services));
    RETURN_IF_FAILED(cdm_services->GetService(
        MF_CONTENTDECRYPTIONMODULE_SERVICE, riid, object_result));
    return S_OK;
  }

  STDMETHODIMP GetInputTrustAuthority(uint32_t stream_id,
                                      uint32_t /*stream_count*/,
                                      const uint8_t* content_init_data,
                                      uint32_t content_init_data_size,
                                      REFIID riid,
                                      IUnknown** object_out) override {
    DVLOG_FUNC(1);

    if (input_trust_authorities_.count(stream_id)) {
      RETURN_IF_FAILED(input_trust_authorities_[stream_id].CopyTo(object_out));
      return S_OK;
    }

    if (!trusted_input_) {
      RETURN_IF_FAILED(mf_cdm_->CreateTrustedInput(
          content_init_data, content_init_data_size, &trusted_input_));
    }

    // GetInputTrustAuthority takes IUnknown* as the output. Using other COM
    // interface will have a v-table mismatch issue.
    ComPtr<IUnknown> unknown;
    RETURN_IF_FAILED(
        trusted_input_->GetInputTrustAuthority(stream_id, riid, &unknown));

    ComPtr<IMFInputTrustAuthority> input_trust_authority;
    RETURN_IF_FAILED(unknown.As(&input_trust_authority));
    RETURN_IF_FAILED(unknown.CopyTo(object_out));

    // Success! Store ITA in the map.
    input_trust_authorities_[stream_id] = input_trust_authority;

    return S_OK;
  }

  STDMETHODIMP SetLastKeyId(uint32_t stream_id, REFGUID key_id) override {
    DVLOG_FUNC(1);
    last_key_ids_[stream_id] = key_id;
    return S_OK;
  }

  STDMETHODIMP RefreshTrustedInput() override {
    DVLOG_FUNC(1);

    // Refresh all decryptors of the last key IDs.
    for (const auto& entry : input_trust_authorities_) {
      const auto& stream_id = entry.first;
      const auto& input_trust_authority = entry.second;

      const auto& last_key_id = last_key_ids_[stream_id];
      if (last_key_id == GUID_NULL)
        continue;

      ComPtr<IMFTransform> decryptor;
      RETURN_IF_FAILED(
          input_trust_authority->GetDecrypter(IID_PPV_ARGS(&decryptor)));
      GUID protection_system_id;
      RETURN_IF_FAILED(GetProtectionSystemId(&protection_system_id));
      RETURN_IF_FAILED(
          RefreshDecryptor(decryptor.Get(), protection_system_id, last_key_id));
    }

    input_trust_authorities_.clear();
    last_key_ids_.clear();
    return S_OK;
  }

  STDMETHODIMP
  ProcessContentEnabler(IUnknown* request, IMFAsyncResult* result) override {
    DVLOG_FUNC(1);
    ComPtr<IMFContentEnabler> content_enabler;
    RETURN_IF_FAILED(request->QueryInterface(IID_PPV_ARGS(&content_enabler)));
    return mf_cdm_->SetContentEnabler(content_enabler.Get(), result);
  }

 private:
  HRESULT GetProtectionSystemId(GUID* protection_system_id) {
    // Typically the CDM should only return one protection system ID. So just
    // use the first one if available.
    base::win::ScopedCoMem<GUID> protection_system_ids;
    DWORD count = 0;
    RETURN_IF_FAILED(
        mf_cdm_->GetProtectionSystemIds(&protection_system_ids, &count));
    if (count == 0)
      return E_FAIL;

    *protection_system_id = *protection_system_ids;
    DVLOG(2) << __func__ << " protection_system_id="
             << base::win::WStringFromGUID(*protection_system_id);

    return S_OK;
  }

  ComPtr<IMFContentDecryptionModule> mf_cdm_;

  // Store IMFTrustedInput to avoid potential performance cost.
  ComPtr<IMFTrustedInput> trusted_input_;

  // |stream_id| to IMFInputTrustAuthority (ITA) mapping. Serves two purposes:
  // 1. The same ITA should always be returned in GetInputTrustAuthority() for
  // the same |stream_id|.
  // 2. The ITA must keep alive for decryptors to work.
  std::map<uint32_t, ComPtr<IMFInputTrustAuthority>> input_trust_authorities_;

  // |stream_id| to last used key ID mapping.
  std::map<uint32_t, GUID> last_key_ids_;
};

}  // namespace

MediaFoundationCdm::MediaFoundationCdm(
    Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mf_cdm,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : mf_cdm_(std::move(mf_cdm)),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  DVLOG_FUNC(1);
  DCHECK(mf_cdm_);
  DCHECK(session_message_cb_);
  DCHECK(session_closed_cb_);
  DCHECK(session_keys_change_cb_);
  DCHECK(session_expiration_update_cb_);
}

MediaFoundationCdm::~MediaFoundationCdm() {
  DVLOG_FUNC(1);
}

void MediaFoundationCdm::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (FAILED(mf_cdm_->SetServerCertificate(certificate.data(),
                                           certificate.size()))) {
    promise->reject(Exception::NOT_SUPPORTED_ERROR, 0, "Failed to set cert");
    return;
  }

  promise->resolve();
}

// TODO(xhwang): Implement this.
void MediaFoundationCdm::GetStatusForPolicy(
    HdcpVersion min_hdcp_version,
    std::unique_ptr<KeyStatusCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "GetStatusForPolicy() is not supported.");
}

void MediaFoundationCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG_FUNC(1);

  // TODO(xhwang): Implement session expiration update.
  auto session = std::make_unique<MediaFoundationCdmSession>(
      session_message_cb_, session_keys_change_cb_,
      session_expiration_update_cb_);

  if (FAILED(session->Initialize(mf_cdm_.Get(), session_type))) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0,
                    "Failed to create session");
    return;
  }

  int session_token = next_session_token_++;

  // Keep a raw pointer since the |promise| will be moved to the callback.
  // Use base::Unretained() is safe because |session| is owned by |this|.
  auto* raw_promise = promise.get();
  auto session_id_cb =
      base::BindOnce(&MediaFoundationCdm::OnSessionId, base::Unretained(this),
                     session_token, std::move(promise));

  if (FAILED(session->GenerateRequest(init_data_type, init_data,
                                      std::move(session_id_cb)))) {
    raw_promise->reject(Exception::INVALID_STATE_ERROR, 0, "Init failure");
    return;
  }

  pending_sessions_.emplace(session_token, std::move(session));
}

void MediaFoundationCdm::LoadSession(
    CdmSessionType session_type,
    const std::string& session_id,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG_FUNC(1);
  NOTIMPLEMENTED();
  promise->reject(Exception::NOT_SUPPORTED_ERROR, 0, "Load not supported");
}

void MediaFoundationCdm::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  auto* session = GetSession(session_id);
  if (!session) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Session not found");
    return;
  }

  if (FAILED(session->Update(response))) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Update failed");
    return;
  }

  promise->resolve();
}

void MediaFoundationCdm::CloseSession(
    const std::string& session_id,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  // Validate that this is a reference to an open session. close() shouldn't
  // be called if the session is already closed. However, the operation is
  // asynchronous, so there is a window where close() was called a second time
  // just before the closed event arrives. As a result it is possible that the
  // session is already closed, so assume that the session is closed if it
  // doesn't exist. https://github.com/w3c/encrypted-media/issues/365.
  //
  // close() is called from a MediaKeySession object, so it is unlikely that
  // this method will be called with a previously unseen |session_id|.
  auto* session = GetSession(session_id);
  if (!session) {
    promise->resolve();
    return;
  }

  if (FAILED(session->Close())) {
    sessions_.erase(session_id);
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Close failed");
    return;
  }

  // EME requires running session closed algorithm before resolving the promise.
  sessions_.erase(session_id);
  session_closed_cb_.Run(session_id);
  promise->resolve();
}

void MediaFoundationCdm::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  auto* session = GetSession(session_id);
  if (!session) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Session not found");
    return;
  }

  if (FAILED(session->Remove())) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Remove failed");
    return;
  }

  promise->resolve();
}

CdmContext* MediaFoundationCdm::GetCdmContext() {
  return this;
}

bool MediaFoundationCdm::RequiresMediaFoundationRenderer() {
  return true;
}

bool MediaFoundationCdm::GetMediaFoundationCdmProxy(
    GetMediaFoundationCdmProxyCB get_mf_cdm_proxy_cb) {
  DVLOG_FUNC(1);

  if (!cdm_proxy_)
    cdm_proxy_ = Make<CdmProxyImpl>(mf_cdm_);

  BindToCurrentLoop(std::move(get_mf_cdm_proxy_cb)).Run(cdm_proxy_);
  return true;
}

bool MediaFoundationCdm::OnSessionId(
    int session_token,
    std::unique_ptr<NewSessionCdmPromise> promise,
    const std::string& session_id) {
  DVLOG_FUNC(1) << "session_token=" << session_token
                << ", session_id=" << session_id;

  auto itr = pending_sessions_.find(session_token);
  DCHECK(itr != pending_sessions_.end());
  auto session = std::move(itr->second);
  DCHECK(session);
  pending_sessions_.erase(itr);

  if (session_id.empty() || sessions_.count(session_id)) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0,
                    "Empty or duplicate session ID");
    return false;
  }

  sessions_.emplace(session_id, std::move(session));
  promise->resolve(session_id);
  return true;
}

MediaFoundationCdmSession* MediaFoundationCdm::GetSession(
    const std::string& session_id) {
  auto itr = sessions_.find(session_id);
  if (itr == sessions_.end())
    return nullptr;

  return itr->second.get();
}

}  // namespace media
