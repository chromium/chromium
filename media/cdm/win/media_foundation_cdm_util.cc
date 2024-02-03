// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_util.h"

#include <combaseapi.h>
#include <initguid.h>  // Needed for DEFINE_PROPERTYKEY to work properly.
#include <mferror.h>
#include <propkeydef.h>  // Needed for DEFINE_PROPERTYKEY.

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/propvarutil.h"
#include "base/win/scoped_propvariant.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/cdm_paths.h"
#include "media/cdm/win/media_foundation_cdm.h"

namespace media {

namespace {

using Microsoft::WRL::ComPtr;

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
    const std::optional<std::vector<uint8_t>>& client_token,
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

}  // namespace

HRESULT CreateMediaFoundationCdm(
    ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const CdmConfig& cdm_config,
    const base::UnguessableToken& cdm_origin_id,
    const std::optional<std::vector<uint8_t>>& cdm_client_token,
    const base::FilePath& cdm_store_path_root,
    ComPtr<IMFContentDecryptionModule>& mf_cdm) {
  DVLOG(1) << __func__ << ": cdm_config=" << cdm_config
           << ", cdm_origin_id=" << cdm_origin_id.ToString()
           << ", cdm_store_path_root=" << cdm_store_path_root;

  DCHECK(!cdm_origin_id.is_empty());

  const auto key_system = cdm_config.key_system;
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

}  // namespace media
