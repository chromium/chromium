// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_factory.h"

#include <combaseapi.h>
#include <mferror.h>
#include <mfmediaengine.h>
#include <propvarutil.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_propvariant.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/media_foundation_cdm.h"

namespace media {

namespace {

using Microsoft::WRL::ComPtr;

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

HRESULT BuildCdmProperties(ComPtr<IPropertyStore>& properties) {
  ComPtr<IPropertyStore> temp_properties;
  RETURN_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(&temp_properties)));

  // TODO(xhwang): Provide per-user, per-profile and per-key-system path here.
  base::FilePath temp_dir;
  CHECK(base::GetTempDir(&temp_dir));
  CHECK(base::DirectoryExists(temp_dir));

  base::win::ScopedPropVariant propvar;
  SetBSTR(temp_dir.value().c_str(), propvar.Receive());
  // TODO(xhwang): Replace with MF_CONTENTDECRYPTIONMODULE_STOREPATH.
  RETURN_IF_FAILED(
      temp_properties->SetValue(MF_EME_CDM_STOREPATH, propvar.get()));

  properties = temp_properties;
  return S_OK;
}

}  // namespace

MediaFoundationCdmFactory::MediaFoundationCdmFactory() = default;

MediaFoundationCdmFactory::~MediaFoundationCdmFactory() = default;

void MediaFoundationCdmFactory::SetCreateCdmFactoryCallback(
    const std::string& key_system,
    CreateCdmFactoryCB create_cdm_factory_cb) {
  DCHECK(!create_cdm_factory_cbs_.count(key_system));
  create_cdm_factory_cbs_[key_system] = std::move(create_cdm_factory_cb);
}

void MediaFoundationCdmFactory::Create(
    const std::string& key_system,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  DVLOG_FUNC(1) << "key_system=" << key_system;

  // IMFContentDecryptionModule CDMs typically require persistent storage and
  // distinctive identifier and this should be guaranteed by key system support
  // code. Update this if there are new CDMs that doesn't require these.
  DCHECK(cdm_config.allow_persistent_state);
  DCHECK(cdm_config.allow_distinctive_identifier);

  ComPtr<IMFContentDecryptionModule> mf_cdm;
  if (FAILED(CreateCdmInternal(key_system, cdm_config, mf_cdm))) {
    BindToCurrentLoop(std::move(cdm_created_cb))
        .Run(nullptr, "Failed to create CDM");
    return;
  }

  auto cdm = base::MakeRefCounted<MediaFoundationCdm>(
      std::move(mf_cdm), session_message_cb, session_closed_cb,
      session_keys_change_cb, session_expiration_update_cb);
  BindToCurrentLoop(std::move(cdm_created_cb)).Run(cdm, "");
}

HRESULT MediaFoundationCdmFactory::CreateMFCdmFactory(
    const std::string& key_system,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& cdm_factory) {
  // Use key system specific `create_cdm_factory_cb` if there's one registered.
  auto itr = create_cdm_factory_cbs_.find(key_system);
  if (itr != create_cdm_factory_cbs_.end()) {
    auto& create_cdm_factory_cb = itr->second;
    if (!create_cdm_factory_cb)
      return E_FAIL;

    RETURN_IF_FAILED(create_cdm_factory_cb.Run(cdm_factory));
    return S_OK;
  }

  // Otherwise, use the default creation.
  ComPtr<IMFMediaEngineClassFactory4> class_factory;
  RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&class_factory)));
  auto key_system_str = base::UTF8ToWide(key_system);
  RETURN_IF_FAILED(class_factory->CreateContentDecryptionModuleFactory(
      key_system_str.c_str(), IID_PPV_ARGS(&cdm_factory)));
  return S_OK;
}

HRESULT MediaFoundationCdmFactory::CreateCdmInternal(
    const std::string& key_system,
    const CdmConfig& cdm_config,
    ComPtr<IMFContentDecryptionModule>& mf_cdm) {
  ComPtr<IMFContentDecryptionModuleFactory> cdm_factory;
  RETURN_IF_FAILED(CreateMFCdmFactory(key_system, cdm_factory));

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

  ComPtr<IPropertyStore> cdm_properties;
  ComPtr<IMFContentDecryptionModule> cdm;
  RETURN_IF_FAILED(BuildCdmProperties(cdm_properties));
  RETURN_IF_FAILED(
      cdm_access->CreateContentDecryptionModule(cdm_properties.Get(), &cdm));

  mf_cdm.Swap(cdm);
  return S_OK;
}

}  // namespace media
