// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_factory.h"

#include <combaseapi.h>
#include <mferror.h>
#include <mfmediaengine.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/media_foundation_cdm.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
#include "media/cdm/win/media_foundation_cdm_util.h"

namespace media {

namespace {

using Microsoft::WRL::ComPtr;

const char kMediaFoundationCdmUmaPrefix[] = "Media.EME.MediaFoundationCdm.";

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
        .Run(nullptr, CreateCdmStatus::kGetCdmPrefDataFailed);
    return;
  }

  if (media_foundation_cdm_data->origin_id.is_empty()) {
    std::move(cdm_created_cb)
        .Run(nullptr, CreateCdmStatus::kGetCdmOriginIdFailed);
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
  auto bound_cdm_created_cb =
      base::BindPostTaskToCurrentDefault(std::move(cdm_created_cb));

  HRESULT hr = cdm->Initialize();

  static bool s_first_initialize_reported = false;
  if (!s_first_initialize_reported) {
    base::UmaHistogramSparse(uma_prefix + "FirstInitialize", hr);
    s_first_initialize_reported = true;
  }

  if (FAILED(hr)) {
    base::UmaHistogramSparse(uma_prefix + "Initialize", hr);
    std::move(bound_cdm_created_cb)
        .Run(nullptr, CreateCdmStatus::kInitCdmFailed);
    return;
  }

  std::move(bound_cdm_created_cb).Run(cdm, CreateCdmStatus::kSuccess);
}

HRESULT MediaFoundationCdmFactory::GetCdmFactory(
    const std::string& key_system,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& cdm_factory) {
  // Use key system specific `create_cdm_factory_cb` if there's one registered.
  auto itr = create_cdm_factory_cbs_for_testing_.find(key_system);
  if (itr != create_cdm_factory_cbs_for_testing_.end()) {
    auto& create_cdm_factory_cb = itr->second;
    DCHECK(create_cdm_factory_cb);
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

void MediaFoundationCdmFactory::OnCdmEvent(CdmEvent event, HRESULT hresult) {
  helper_->OnCdmEvent(event, hresult);
}

void MediaFoundationCdmFactory::CreateMfCdm(
    const CdmConfig& cdm_config,
    const base::UnguessableToken& cdm_origin_id,
    const std::optional<std::vector<uint8_t>>& cdm_client_token,
    const base::FilePath& cdm_store_path_root,
    HRESULT& hresult,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModule>& mf_cdm) {
  ComPtr<IMFContentDecryptionModuleFactory> cdm_factory;
  hresult = GetCdmFactory(cdm_config.key_system, cdm_factory);
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to GetCdmFactory. hr=" << hresult;
    return;
  }

  hresult =
      CreateMediaFoundationCdm(cdm_factory, cdm_config, cdm_origin_id,
                               cdm_client_token, cdm_store_path_root, mf_cdm);
}

}  // namespace media
