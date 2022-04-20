// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_factory.h"

#include <combaseapi.h>
#include <initguid.h>  // Needed for DEFINE_PROPERTYKEY to work properly.
#include <mferror.h>
#include <mfmediaengine.h>
#include <propkeydef.h>  // Needed for DEFINE_PROPERTYKEY.
#include <propvarutil.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/win/scoped_propvariant.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/cdm_paths.h"
#include "media/cdm/win/media_foundation_cdm.h"
#include "media/cdm/win/media_foundation_cdm_module.h"

namespace media {

namespace {

using Microsoft::WRL::ComPtr;

const char kMediaFoundationCdmUmaPrefix[] = "Media.EME.MediaFoundationCdm.";

// Key to the CDM Origin ID to be passed to the CDM for privacy purposes. The
// same value is also used in MediaFoundation CDMs. Do NOT change this value!
DEFINE_PROPERTYKEY(EME_CONTENTDECRYPTIONMODULE_ORIGIN_ID,
                   0x1218a3e2,
                   0xcfb0,
                   0x4c98,
                   0x90,
                   0xe5,
                   0x5f,
                   0x58,
                   0x18,
                   0xd4,
                   0xb6,
                   0x7e,
                   PID_FIRST_USABLE);

void SetBSTR(const wchar_t* str, PROPVARIANT* propvariant) {
  propvariant->vt = VT_BSTR;
  propvariant->bstrVal = SysAllocString(str);
}

// Returns a property store similar to EME MediaKeySystemMediaCapability.
HRESULT CreateVideoCapability(const CdmConfig& cdm_config,
                              ComPtr<IPropertyStore>& video_capability) {
  ComPtr<IPropertyStore> temp_video_capability;
  RETURN_IF_FAILED(
      PSCreateMemoryPropertyStore(IID_PPV_ARGS(&temp_video_capability)));

  base::win::ScopedPropVariant robustness;
  if (cdm_config.use_hw_secure_codecs) {
    // TODO(xhwang): Provide a way to support other robustness strings.
    SetBSTR(L"HW_SECURE_ALL", robustness.Receive());
    RETURN_IF_FAILED(
        temp_video_capability->SetValue(MF_EME_ROBUSTNESS, robustness.get()));
  }
  video_capability = temp_video_capability;
  return S_OK;
}

// Returns a property store similar to EME MediaKeySystemConfigurations.
// What really matters here are video robustness, persistent state and
// distinctive identifier.
HRESULT BuildCdmAccessConfigurations(const CdmConfig& cdm_config,
                                     ComPtr<IPropertyStore>& configurations) {
  ComPtr<IPropertyStore> temp_configurations;

  RETURN_IF_FAILED(
      PSCreateMemoryPropertyStore(IID_PPV_ARGS(&temp_configurations)));

  // Add an empty audio capability.
  base::win::ScopedPropVariant audio_capabilities;
  PROPVARIANT* var_to_set = audio_capabilities.Receive();
  var_to_set->vt = VT_VARIANT | VT_VECTOR;
  var_to_set->capropvar.cElems = 0;
  RETURN_IF_FAILED(temp_configurations->SetValue(MF_EME_AUDIOCAPABILITIES,
                                                 audio_capabilities.get()));

  // Add a video capability so we can pass the correct robustness level.
  ComPtr<IPropertyStore> video_capability;
  RETURN_IF_FAILED(CreateVideoCapability(cdm_config, video_capability));

  base::win::ScopedPropVariant video_config;
  auto* video_config_ptr = video_config.Receive();
  video_config_ptr->vt = VT_UNKNOWN;
  video_config_ptr->punkVal = video_capability.Detach();

  base::win::ScopedPropVariant video_capabilities;
  var_to_set = video_capabilities.Receive();
  var_to_set->vt = VT_VARIANT | VT_VECTOR;
  var_to_set->capropvar.cElems = 1;
  var_to_set->capropvar.pElems =
      reinterpret_cast<PROPVARIANT*>(CoTaskMemAlloc(sizeof(PROPVARIANT)));
  PropVariantCopy(var_to_set->capropvar.pElems, video_config.ptr());
  RETURN_IF_FAILED(temp_configurations->SetValue(MF_EME_VIDEOCAPABILITIES,
                                                 video_capabilities.get()));

  // Add persistent state.
  DCHECK(cdm_config.allow_persistent_state);
  base::win::ScopedPropVariant persisted_state;
  RETURN_IF_FAILED(InitPropVariantFromUInt32(MF_MEDIAKEYS_REQUIREMENT_REQUIRED,
                                             persisted_state.Receive()));
  RETURN_IF_FAILED(temp_configurations->SetValue(MF_EME_PERSISTEDSTATE,
                                                 persisted_state.get()));

  // Add distinctive identifier.
  DCHECK(cdm_config.allow_distinctive_identifier);
  base::win::ScopedPropVariant distinctive_identifier;
  RETURN_IF_FAILED(InitPropVariantFromUInt32(MF_MEDIAKEYS_REQUIREMENT_REQUIRED,
                                             distinctive_identifier.Receive()));
  RETURN_IF_FAILED(temp_configurations->SetValue(MF_EME_DISTINCTIVEID,
                                                 distinctive_identifier.get()));

  configurations = temp_configurations;
  return S_OK;
}

HRESULT BuildCdmProperties(
    const base::UnguessableToken& origin_id,
    const absl::optional<std::vector<uint8_t>>& client_token,
    const base::FilePath& store_path,
    ComPtr<IPropertyStore>& properties) {
  DCHECK(!origin_id.is_empty());

  ComPtr<IPropertyStore> temp_properties;
  RETURN_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(&temp_properties)));

  base::win::ScopedPropVariant origin_id_var;
  RETURN_IF_FAILED(InitPropVariantFromString(
      base::UTF8ToWide(origin_id.ToString()).c_str(), origin_id_var.Receive()));
  RETURN_IF_FAILED(temp_properties->SetValue(
      EME_CONTENTDECRYPTIONMODULE_ORIGIN_ID, origin_id_var.get()));

  if (client_token) {
    base::win::ScopedPropVariant client_token_var;
    PROPVARIANT* client_token_propvar = client_token_var.Receive();
    client_token_propvar->vt = VT_VECTOR | VT_UI1;
    client_token_propvar->caub.cElems = client_token->size();
    client_token_propvar->caub.pElems = reinterpret_cast<unsigned char*>(
        CoTaskMemAlloc(client_token->size() * sizeof(char)));
    memcpy(client_token_propvar->caub.pElems, client_token->data(),
           client_token->size());

    RETURN_IF_FAILED(temp_properties->SetValue(
        EME_CONTENTDECRYPTIONMODULE_CLIENT_TOKEN, client_token_var.get()));
  }

  base::win::ScopedPropVariant store_path_var;
  RETURN_IF_FAILED(InitPropVariantFromString(store_path.value().c_str(),
                                             store_path_var.Receive()));
  RETURN_IF_FAILED(temp_properties->SetValue(
      MF_CONTENTDECRYPTIONMODULE_STOREPATH, store_path_var.get()));

  properties = temp_properties;
  return S_OK;
}

bool IsTypeSupportedInternal(
    ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const std::string& key_system,
    const std::string& content_type) {
  return cdm_factory->IsTypeSupported(base::UTF8ToWide(key_system).c_str(),
                                      base::UTF8ToWide(content_type).c_str());
}

crash_reporter::CrashKeyString<256> g_origin_crash_key("cdm-origin");

}  // namespace

MediaFoundationCdmFactory::MediaFoundationCdmFactory(
    std::unique_ptr<CdmAuxiliaryHelper> helper)
    : helper_(std::move(helper)),
      cdm_origin_crash_key_(&g_origin_crash_key,
                            helper_->GetCdmOrigin().Serialize()) {}

MediaFoundationCdmFactory::~MediaFoundationCdmFactory() = default;

void MediaFoundationCdmFactory::SetCreateCdmFactoryCallbackForTesting(
    const std::string& key_system,
    CreateCdmFactoryCB create_cdm_factory_cb) {
  DCHECK(!create_cdm_factory_cbs_for_testing_.count(key_system));
  create_cdm_factory_cbs_for_testing_[key_system] =
      std::move(create_cdm_factory_cb);
}

void MediaFoundationCdmFactory::Create(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  DVLOG_FUNC(1) << "cdm_config=" << cdm_config;

  // IMFContentDecryptionModule CDMs typically require persistent storage and
  // distinctive identifier and this should be guaranteed by key system support
  // code. Update this if there are new CDMs that doesn't require these.
  DCHECK(cdm_config.allow_persistent_state);
  DCHECK(cdm_config.allow_distinctive_identifier);

  // Don't cache `cdm_origin_id` in this class since user can clear it any time.
  helper_->GetMediaFoundationCdmData(
      base::BindOnce(&MediaFoundationCdmFactory::OnCdmOriginIdObtained,
                     weak_factory_.GetWeakPtr(), cdm_config, session_message_cb,
                     session_closed_cb, session_keys_change_cb,
                     session_expiration_update_cb, std::move(cdm_created_cb)));
}

void MediaFoundationCdmFactory::OnCdmOriginIdObtained(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb,
    const std::unique_ptr<MediaFoundationCdmData> media_foundation_cdm_data) {
  if (!media_foundation_cdm_data) {
    std::move(cdm_created_cb)
        .Run(nullptr, "Failed to get the CDM preference data.");
    return;
  }

  if (media_foundation_cdm_data->origin_id.is_empty()) {
    std::move(cdm_created_cb).Run(nullptr, "Failed to get the CDM origin ID.");
    return;
  }

  // This will construct a UMA prefix to be something like (with trailing dot):
  // "Media.EME.MediaFoundationCdm.FooKeySystem.HardwareSecure.".
  auto uma_prefix = kMediaFoundationCdmUmaPrefix +
                    GetKeySystemNameForUMA(cdm_config.key_system,
                                           cdm_config.use_hw_secure_codecs) +
                    ".";

  auto cdm = base::MakeRefCounted<MediaFoundationCdm>(
      uma_prefix,
      base::BindRepeating(&MediaFoundationCdmFactory::CreateMfCdm,
                          weak_factory_.GetWeakPtr(), cdm_config,
                          media_foundation_cdm_data->origin_id,
                          media_foundation_cdm_data->client_token,
                          media_foundation_cdm_data->cdm_store_path_root),
      base::BindRepeating(&MediaFoundationCdmFactory::IsTypeSupported,
                          weak_factory_.GetWeakPtr(), cdm_config.key_system),
      base::BindRepeating(&MediaFoundationCdmFactory::StoreClientToken,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaFoundationCdmFactory::OnCdmEvent,
                          weak_factory_.GetWeakPtr()),
      session_message_cb, session_closed_cb, session_keys_change_cb,
      session_expiration_update_cb);

  // `cdm_created_cb` should always be run asynchronously.
  auto bound_cdm_created_cb = BindToCurrentLoop(std::move(cdm_created_cb));

  HRESULT hr = cdm->Initialize();

  static bool s_first_initialize_reported = false;
  if (!s_first_initialize_reported) {
    base::UmaHistogramSparse(uma_prefix + "FirstInitialize", hr);
    s_first_initialize_reported = true;
  }

  if (FAILED(hr)) {
    base::UmaHistogramSparse(uma_prefix + "Initialize", hr);
    std::move(bound_cdm_created_cb)
        .Run(nullptr, "Failed to initialize CDM: " + PrintHr(hr));
    return;
  }

  std::move(bound_cdm_created_cb).Run(cdm, "");
}

HRESULT MediaFoundationCdmFactory::GetCdmFactory(
    const std::string& key_system,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& cdm_factory) {
  // Use key system specific `create_cdm_factory_cb` if there's one registered.
  auto itr = create_cdm_factory_cbs_for_testing_.find(key_system);
  if (itr != create_cdm_factory_cbs_for_testing_.end()) {
    auto& create_cdm_factory_cb = itr->second;
    if (!create_cdm_factory_cb)
      return E_FAIL;

    RETURN_IF_FAILED(create_cdm_factory_cb.Run(cdm_factory));
    return S_OK;
  }

  // Otherwise, use the one in MediaFoundationCdmModule.
  RETURN_IF_FAILED(MediaFoundationCdmModule::GetInstance()->GetCdmFactory(
      key_system, cdm_factory));
  return S_OK;
}

void MediaFoundationCdmFactory::IsTypeSupported(
    const std::string& key_system,
    const std::string& content_type,
    IsTypeSupportedResultCB is_type_supported_result_cb) {
  ComPtr<IMFContentDecryptionModuleFactory> cdm_factory;
  HRESULT hr = GetCdmFactory(key_system, cdm_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to GetCdmFactory. hr=" << hr;
    std::move(is_type_supported_result_cb).Run(false);
    return;
  }

  // Note that IsTypeSupported may take up to 10s, so run it on a separate
  // thread to unblock the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&IsTypeSupportedInternal, cdm_factory, key_system,
                     content_type),
      std::move(is_type_supported_result_cb));
}

void MediaFoundationCdmFactory::StoreClientToken(
    const std::vector<uint8_t>& client_token) {
  helper_->SetCdmClientToken(client_token);
}

void MediaFoundationCdmFactory::OnCdmEvent(CdmEvent event) {
  helper_->OnCdmEvent(event);
}

HRESULT MediaFoundationCdmFactory::CreateMfCdmInternal(
    const CdmConfig& cdm_config,
    const base::UnguessableToken& cdm_origin_id,
    const absl::optional<std::vector<uint8_t>>& cdm_client_token,
    const base::FilePath& cdm_store_path_root,
    ComPtr<IMFContentDecryptionModule>& mf_cdm) {
  const auto key_system = cdm_config.key_system;
  ComPtr<IMFContentDecryptionModuleFactory> cdm_factory;
  RETURN_IF_FAILED(GetCdmFactory(key_system, cdm_factory));

  DCHECK(!cdm_origin_id.is_empty());

  auto key_system_str = base::UTF8ToWide(key_system);
  if (!cdm_factory->IsTypeSupported(key_system_str.c_str(), nullptr)) {
    DLOG(ERROR) << key_system << " not supported by MF CdmFactory";
    return MF_NOT_SUPPORTED_ERR;
  }

  ComPtr<IPropertyStore> property_store;
  RETURN_IF_FAILED(BuildCdmAccessConfigurations(cdm_config, property_store));

  IPropertyStore* configurations[] = {property_store.Get()};
  ComPtr<IMFContentDecryptionModuleAccess> cdm_access;
  RETURN_IF_FAILED(cdm_factory->CreateContentDecryptionModuleAccess(
      key_system_str.c_str(), configurations, ARRAYSIZE(configurations),
      &cdm_access));

  // Provide a per-user, per-arch, per-origin and per-key-system path.
  auto store_path =
      GetCdmStorePath(cdm_store_path_root, cdm_origin_id, key_system);
  DVLOG(1) << "store_path=" << store_path;

  // Ensure the path exists. If it already exists, this call will do nothing.
  base::File::Error file_error;
  if (!base::CreateDirectoryAndGetError(store_path, &file_error)) {
    DLOG(ERROR) << "Create CDM store path failed with " << file_error;
    return MF_INVALID_ACCESS_ERR;
  }

  ComPtr<IPropertyStore> cdm_properties;
  ComPtr<IMFContentDecryptionModule> cdm;
  RETURN_IF_FAILED(BuildCdmProperties(cdm_origin_id, cdm_client_token,
                                      store_path, cdm_properties));
  RETURN_IF_FAILED(
      cdm_access->CreateContentDecryptionModule(cdm_properties.Get(), &cdm));

  mf_cdm.Swap(cdm);
  return S_OK;
}

void MediaFoundationCdmFactory::CreateMfCdm(
    const CdmConfig& cdm_config,
    const base::UnguessableToken& cdm_origin_id,
    const absl::optional<std::vector<uint8_t>>& cdm_client_token,
    const base::FilePath& cdm_store_path_root,
    HRESULT& hresult,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModule>& mf_cdm) {
  hresult = CreateMfCdmInternal(cdm_config, cdm_origin_id, cdm_client_token,
                                cdm_store_path_root, mf_cdm);
}

}  // namespace media
