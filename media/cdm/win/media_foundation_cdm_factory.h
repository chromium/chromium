// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_FACTORY_H_
#define MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_FACTORY_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl.h>

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_com_initializer.h"
#include "components/crash/core/common/crash_key.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_auxiliary_helper.h"

namespace media {

class MEDIA_EXPORT MediaFoundationCdmFactory final : public CdmFactory {
 public:
  MediaFoundationCdmFactory(std::unique_ptr<CdmAuxiliaryHelper> helper);
  MediaFoundationCdmFactory(const MediaFoundationCdmFactory&) = delete;
  MediaFoundationCdmFactory& operator=(const MediaFoundationCdmFactory&) =
      delete;
  ~MediaFoundationCdmFactory() override;

  // Provides a way to customize IMFContentDecryptionModuleFactory creation to
  // support different key systems and for testing.
  using CreateCdmFactoryCB = base::RepeatingCallback<HRESULT(
      Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& factory)>;
  void SetCreateCdmFactoryCallbackForTesting(
      const std::string& key_system,
      CreateCdmFactoryCB create_cdm_factory_cb);

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override;

 private:
  // Callback to MediaFoundationCDM to resolve the promise.
  using IsTypeSupportedResultCB = base::OnceCallback<void(bool is_supported)>;

  void OnCdmOriginIdObtained(
      const CdmConfig& cdm_config,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb,
      CdmCreatedCB cdm_created_cb,
      const std::unique_ptr<MediaFoundationCdmData> media_foundation_cdm_data);

  HRESULT GetCdmFactory(
      const std::string& key_system,
      Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& cdm_factory);

  void IsTypeSupported(const std::string& key_system,
                       const std::string& content_type,
                       IsTypeSupportedResultCB is_type_supported_result_cb);

  void StoreClientToken(const std::vector<uint8_t>& client_token);
  void OnCdmEvent(CdmEvent event, HRESULT hresult);

  // Creates `mf_cdm` based on the input parameters. Same as
  // CreateMediaFoundationCdm() but returns the HRESULT in out parameter so we
  // can bind it to a repeating callback using weak pointer.
  void CreateMfCdm(const CdmConfig& cdm_config,
                   const base::UnguessableToken& cdm_origin_id,
                   const std::optional<std::vector<uint8_t>>& cdm_client_token,
                   const base::FilePath& cdm_store_path_root,
                   HRESULT& hresult,
                   Microsoft::WRL::ComPtr<IMFContentDecryptionModule>& mf_cdm);

  std::unique_ptr<CdmAuxiliaryHelper> helper_;

  // CDM origin crash key used in crash reporting.
  crash_reporter::ScopedCrashKeyString cdm_origin_crash_key_;

  // IMFContentDecryptionModule implementations typically require MTA to run.
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};

  // Key system to CreateCdmFactoryCB mapping. This is for testing only.
  std::map<std::string, CreateCdmFactoryCB> create_cdm_factory_cbs_for_testing_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationCdmFactory> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_FACTORY_H_
