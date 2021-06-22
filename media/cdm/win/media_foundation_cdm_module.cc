// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_module.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_hstring.h"
#include "media/base/win/mf_helpers.h"

namespace media {

namespace {

using Microsoft::WRL::ComPtr;

static MediaFoundationCdmModule* g_cdm_module = nullptr;

}  // namespace

// static
MediaFoundationCdmModule* MediaFoundationCdmModule::GetInstance() {
  if (!g_cdm_module)
    g_cdm_module = new MediaFoundationCdmModule();

  return g_cdm_module;
}

MediaFoundationCdmModule::MediaFoundationCdmModule() = default;
MediaFoundationCdmModule::~MediaFoundationCdmModule() = default;

void MediaFoundationCdmModule::Initialize(const base::FilePath& cdm_path) {
  DVLOG(1) << __func__ << ": cdm_path=" << cdm_path.value();
  CHECK(!initialized_)
      << "MediaFoundationCdmModule can only be initialized once!";

  initialized_ = true;
  cdm_path_ = cdm_path;

  // If `cdm_path_` is not empty, load the CDM before the sandbox is sealed.
  if (!cdm_path_.empty()) {
    library_ = base::ScopedNativeLibrary(cdm_path_);
    LOG_IF(ERROR, !library_.is_valid())
        << __func__ << ": Failed to load CDM at " << cdm_path_.value()
        << " (Error: " << library_.GetError()->ToString() << ")";
  }
}

HRESULT MediaFoundationCdmModule::GetCdmFactory(
    const std::string& key_system,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& cdm_factory) {
  if (!initialized_) {
    DLOG(ERROR) << __func__ << " failed: Not initialized";
    return E_NOT_VALID_STATE;
  }

  if (key_system.empty()) {
    DLOG(ERROR) << __func__ << " failed: Empty key system";
    return ERROR_INVALID_PARAMETER;
  }

  if (key_system_.empty())
    key_system_ = key_system;

  if (key_system != key_system_) {
    DLOG(ERROR) << __func__ << " failed: key system mismatch";
    return E_NOT_VALID_STATE;
  }

  if (!cdm_factory_)
    RETURN_IF_FAILED(ActivateCdmFactory());

  cdm_factory = cdm_factory_;
  return S_OK;
}

HRESULT MediaFoundationCdmModule::ActivateCdmFactory() {
  DCHECK(initialized_);

  // For OS or store CDM, the `cdm_path_` is empty. Just use default creation.
  if (cdm_path_.empty()) {
    DCHECK(!library_.is_valid());
    ComPtr<IMFMediaEngineClassFactory4> class_factory;
    RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                                      CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&class_factory)));
    auto key_system_str = base::UTF8ToWide(key_system_);
    RETURN_IF_FAILED(class_factory->CreateContentDecryptionModuleFactory(
        key_system_str.c_str(), IID_PPV_ARGS(&cdm_factory_)));
    return S_OK;
  }

  if (!library_.is_valid()) {
    LOG(ERROR) << "CDM failed to load previously";
    return E_FAIL;
  }

  // Initialization required to call base::win::ScopedHString::Create();
  if (!base::win::ScopedHString::ResolveCoreWinRTStringDelayload())
    return E_FAIL;

  // Get function pointer to the activation factory.
  using GetActivationFactoryFunc =
      HRESULT(WINAPI*)(_In_ HSTRING activatible_class_id,
                       _COM_Outptr_ IActivationFactory * *factory);
  const char kDllGetActivationFactory[] = "DllGetActivationFactory";
  auto get_activation_factory_func = reinterpret_cast<GetActivationFactoryFunc>(
      library_.GetFunctionPointer(kDllGetActivationFactory));
  if (!get_activation_factory_func) {
    LOG(ERROR) << "Cannot get function " << kDllGetActivationFactory;
    return E_FAIL;
  }

  // Activate CdmFactory. Assuming the class ID is always in the format
  // "<key_system>.ContentDecryptionModuleFactory".
  auto class_name = base::win::ScopedHString::Create(
      base::StringPiece(key_system_ + ".ContentDecryptionModuleFactory"));
  ComPtr<IActivationFactory> activation_factory;
  RETURN_IF_FAILED(
      get_activation_factory_func(class_name.get(), &activation_factory));

  ComPtr<IInspectable> inspectable_factory;
  RETURN_IF_FAILED(activation_factory->ActivateInstance(&inspectable_factory));
  RETURN_IF_FAILED(inspectable_factory.As(&cdm_factory_));

  return S_OK;
}

}  // namespace media
