// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_cdm.h"

#include <mfapi.h>
#include <mferror.h>
#include <windows.media.protection.playready.h>
#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/notreached.h"
#include "media/base/win/mf_feature_checks.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"
#include "media/cdm/win/test/media_foundation_clear_key_session.h"
#include "media/cdm/win/test/media_foundation_clear_key_trusted_input.h"
#include "media/cdm/win/test/mock_media_protection_pmp_server.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

static HRESULT AddPropertyToSet(
    _Inout_ ABI::Windows::Foundation::Collections::IPropertySet* property_set,
    _In_ LPCWSTR name,
    _In_ IInspectable* inspectable) {
  boolean replaced = false;
  ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>>
      map;

  RETURN_IF_FAILED(property_set->QueryInterface(IID_PPV_ARGS(&map)));
  RETURN_IF_FAILED(
      map->Insert(Microsoft::WRL::Wrappers::HStringReference(name).Get(),
                  inspectable, &replaced));

  return S_OK;
}

static HRESULT AddStringToPropertySet(
    _Inout_ ABI::Windows::Foundation::Collections::IPropertySet* property_set,
    _In_ LPCWSTR name,
    _In_ LPCWSTR string) {
  ComPtr<ABI::Windows::Foundation::IPropertyValue> property_value;
  ComPtr<ABI::Windows::Foundation::IPropertyValueStatics>
      property_value_statics;

  RETURN_IF_FAILED(ABI::Windows::Foundation::GetActivationFactory(
      Microsoft::WRL::Wrappers::HStringReference(
          RuntimeClass_Windows_Foundation_PropertyValue)
          .Get(),
      &property_value_statics));

  RETURN_IF_FAILED(property_value_statics->CreateString(
      Microsoft::WRL::Wrappers::HStringReference(string).Get(),
      &property_value));
  RETURN_IF_FAILED(AddPropertyToSet(property_set, name, property_value.Get()));

  return S_OK;
}

static HRESULT AddBoolToPropertySet(
    _Inout_ ABI::Windows::Foundation::Collections::IPropertySet* property_set,
    _In_ LPCWSTR name,
    _In_ BOOL value) {
  ComPtr<ABI::Windows::Foundation::IPropertyValue> property_value;
  ComPtr<ABI::Windows::Foundation::IPropertyValueStatics>
      property_value_statics;

  RETURN_IF_FAILED(ABI::Windows::Foundation::GetActivationFactory(
      Microsoft::WRL::Wrappers::HStringReference(
          RuntimeClass_Windows_Foundation_PropertyValue)
          .Get(),
      &property_value_statics));

  RETURN_IF_FAILED(
      property_value_statics->CreateBoolean(!!value, &property_value));
  RETURN_IF_FAILED(AddPropertyToSet(property_set, name, property_value.Get()));

  return S_OK;
}

}  // namespace

MediaFoundationClearKeyCdm::MediaFoundationClearKeyCdm() {
  DVLOG_FUNC(1);
}

MediaFoundationClearKeyCdm::~MediaFoundationClearKeyCdm() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Shutdown();
}

HRESULT MediaFoundationClearKeyCdm::RuntimeClassInitialize(
    _In_ IPropertyStore* properties) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ComPtr<ABI::Windows::Foundation::Collections::IPropertySet> property_pmp;
  RETURN_IF_FAILED(Windows::Foundation::ActivateInstance(
      Microsoft::WRL::Wrappers::HStringReference(
          RuntimeClass_Windows_Foundation_Collections_PropertySet)
          .Get(),
      &property_pmp));

  // As a workaround to create an in-process PMP server, use the PlayReady media
  // protection system ID here as the MediaEngine will call
  // MFIsContentProtectionDeviceSupported() to determine whether the specified
  // protection system ID is supported.
  RETURN_IF_FAILED(AddStringToPropertySet(
      property_pmp.Get(), L"Windows.Media.Protection.MediaProtectionSystemId",
      PLAYREADY_GUID_MEDIA_PROTECTION_SYSTEM_ID_STRING));

  // Setting this to TRUE allows the system to create an in-process PMP server,
  // pretending to use hardware protection layer.
  RETURN_IF_FAILED(AddBoolToPropertySet(
      property_pmp.Get(),
      L"Windows.Media.Protection.UseHardwareProtectionLayer", TRUE));

  // Note that we don't need to add this property
  // "Windows.Media.Protection.MediaProtectionSystemIdMapping".

  // Use a custom PMP server so that MediaEngine can create an in-process PMP
  // server regardless of the system's hardware decryption capability.
  RETURN_IF_FAILED((MakeAndInitialize<
                    MockMediaProtectionPMPServer,
                    ABI::Windows::Media::Protection::IMediaProtectionPMPServer>(
      &media_protection_pmp_server_, property_pmp.Get())));

  return S_OK;
}

// IMFContentDecryptionModule
STDMETHODIMP MediaFoundationClearKeyCdm::SetContentEnabler(
    _In_ IMFContentEnabler* content_enabler,
    _In_ IMFAsyncResult* result) {
  DVLOG_FUNC(1);

  // This method can be called from a different MF thread, so the
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) is not checked here.

  RETURN_IF_FAILED(GetShutdownStatus());

  if (!content_enabler || !result) {
    return E_INVALIDARG;
  }

  // Invoke the callback immediately but will determine whether the keyid exists
  // or not in the decryptor's ProcessOutput().
  RETURN_IF_FAILED(MFInvokeCallback(result));

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdm::GetSuspendNotify(
    _COM_Outptr_ IMFCdmSuspendNotify** notify) {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdm::SetPMPHostApp(IMFPMPHostApp* host) {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdm::CreateSession(
    _In_ MF_MEDIAKEYSESSION_TYPE session_type,
    _In_ IMFContentDecryptionModuleSessionCallbacks* callbacks,
    _COM_Outptr_ IMFContentDecryptionModuleSession** session) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RETURN_IF_FAILED(GetShutdownStatus());

  RETURN_IF_FAILED((MakeAndInitialize<MediaFoundationClearKeySession,
                                      IMFContentDecryptionModuleSession>(
      session, session_type, callbacks, GetAesDecryptor(),
      base::BindOnce(&MediaFoundationClearKeyCdm::OnSessionIdCreated,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&MediaFoundationClearKeyCdm::OnSessionIdRemoved,
                     weak_factory_.GetWeakPtr()))));

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdm::SetServerCertificate(
    _In_reads_bytes_opt_(server_certificate_size)
        const BYTE* server_certificate,
    _In_ DWORD server_certificate_size) {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdm::CreateTrustedInput(
    _In_reads_bytes_(content_init_data_size) const BYTE* content_init_data,
    _In_ DWORD content_init_data_size,
    _COM_Outptr_ IMFTrustedInput** trusted_input) {
  DVLOG_FUNC(1);

  // This method can be called from a different MF thread, so the
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) is not checked here.

  RETURN_IF_FAILED(GetShutdownStatus());

  ComPtr<IMFTrustedInput> trusted_input_new;
  RETURN_IF_FAILED(
      (MakeAndInitialize<MediaFoundationClearKeyTrustedInput, IMFTrustedInput>(
          &trusted_input_new, GetAesDecryptor())));

  *trusted_input = trusted_input_new.Detach();

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdm::GetProtectionSystemIds(
    _Outptr_result_buffer_(*count) GUID** system_ids,
    _Out_ DWORD* count) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RETURN_IF_FAILED(GetShutdownStatus());

  *system_ids = nullptr;
  *count = 0;

  GUID* system_id = static_cast<GUID*>(CoTaskMemAlloc(sizeof(GUID)));
  if (!system_id) {
    return E_OUTOFMEMORY;
  }

  *system_id = MEDIA_FOUNDATION_CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID;
  *system_ids = system_id;
  *count = 1;

  return S_OK;
}

// IMFGetService
STDMETHODIMP MediaFoundationClearKeyCdm::GetService(
    __RPC__in REFGUID guid_service,
    __RPC__in REFIID riid,
    __RPC__deref_out_opt LPVOID* object) {
  DVLOG_FUNC(1);

  // This method can be called from a different MF thread, so the
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) is not checked here.

  RETURN_IF_FAILED(GetShutdownStatus());

  if (MF_CONTENTDECRYPTIONMODULE_SERVICE != guid_service) {
    return MF_E_UNSUPPORTED_SERVICE;
  }

  if (media_protection_pmp_server_ == nullptr) {
    return MF_INVALID_STATE_ERR;
  }

  if (riid == ABI::Windows::Media::Protection::IID_IMediaProtectionPMPServer) {
    RETURN_IF_FAILED(media_protection_pmp_server_.CopyTo(riid, object));
  } else {
    ComPtr<IMFGetService> get_service;
    RETURN_IF_FAILED(media_protection_pmp_server_.As(&get_service));
    RETURN_IF_FAILED(get_service->GetService(MF_PMP_SERVICE, riid, object));
  }

  return S_OK;
}

// IMFShutdown
STDMETHODIMP MediaFoundationClearKeyCdm::Shutdown() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::AutoLock lock(lock_);
  if (is_shutdown_) {
    return MF_E_SHUTDOWN;
  }

  is_shutdown_ = true;
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdm::GetShutdownStatus(
    MFSHUTDOWN_STATUS* status) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Per IMFShutdown::GetShutdownStatus spec, MF_E_INVALIDREQUEST is returned if
  // Shutdown has not been called beforehand.
  base::AutoLock lock(lock_);
  if (!is_shutdown_) {
    return MF_E_INVALIDREQUEST;
  }

  return S_OK;
}

scoped_refptr<AesDecryptor> MediaFoundationClearKeyCdm::GetAesDecryptor() {
  DVLOG_FUNC(1);

  if (!aes_decryptor_) {
    aes_decryptor_ = base::MakeRefCounted<AesDecryptor>(
        base::BindRepeating(&MediaFoundationClearKeyCdm::OnSessionMessage,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&MediaFoundationClearKeyCdm::OnSessionClosed,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&MediaFoundationClearKeyCdm::OnSessionKeysChange,
                            weak_factory_.GetWeakPtr()),
        base::DoNothing());  // AesDecryptor never calls this.
  }

  return aes_decryptor_;
}

void MediaFoundationClearKeyCdm::OnSessionMessage(
    const std::string& session_id,
    CdmMessageType message_type,
    const std::vector<uint8_t>& message) {
  DVLOG_FUNC(1) << "session_id=" << session_id
                << ", message_type=" << static_cast<int>(message_type);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto* session = FindSession(session_id);
  CHECK(session);
  session->OnSessionMessage(session_id, message_type, message);
}

void MediaFoundationClearKeyCdm::OnSessionClosed(
    const std::string& session_id,
    CdmSessionClosedReason reason) {
  DVLOG_FUNC(1) << "session_id=" << session_id
                << ", reason=" << static_cast<int>(reason);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto* session = FindSession(session_id);
  CHECK(session);
  session->OnSessionClosed(session_id, reason);
}

void MediaFoundationClearKeyCdm::OnSessionKeysChange(
    const std::string& session_id,
    bool has_additional_usable_key,
    CdmKeysInfo keys_info) {
  DVLOG_FUNC(1) << "session_id=" << session_id
                << ", has_additional_usable_key=" << has_additional_usable_key;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto* session = FindSession(session_id);
  CHECK(session);
  session->OnSessionKeysChange(session_id, has_additional_usable_key,
                               std::move(keys_info));
}

void MediaFoundationClearKeyCdm::OnSessionIdCreated(
    const std::string& session_id,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession> session) {
  DVLOG_FUNC(1) << "session_id=" << session_id;
  CHECK(FindSession(session_id) == nullptr);
  CHECK(session);

  sessions_.emplace(session_id, session);
}

void MediaFoundationClearKeyCdm::OnSessionIdRemoved(
    const std::string& session_id) {
  DVLOG_FUNC(1) << "session_id=" << session_id;
  auto it = sessions_.find(session_id);
  CHECK(it != sessions_.end());
  sessions_.erase(it);
}

MediaFoundationClearKeySession* MediaFoundationClearKeyCdm::FindSession(
    const std::string& session_id) {
  DVLOG_FUNC(3) << "session_id=" << session_id;
  auto it = sessions_.find(session_id);
  return it == sessions_.end()
             ? nullptr
             : static_cast<MediaFoundationClearKeySession*>(it->second.Get());
}

}  // namespace media
