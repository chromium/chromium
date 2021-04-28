// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_FACTORY_H_
#define MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_FACTORY_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_com_initializer.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_auxiliary_helper.h"

namespace media {

class MEDIA_EXPORT MediaFoundationCdmFactory : public CdmFactory {
 public:
  MediaFoundationCdmFactory(std::unique_ptr<CdmAuxiliaryHelper> helper,
                            const base::FilePath& user_data_dir);
  MediaFoundationCdmFactory(const MediaFoundationCdmFactory&) = delete;
  MediaFoundationCdmFactory& operator=(const MediaFoundationCdmFactory&) =
      delete;
  ~MediaFoundationCdmFactory() final;

  // Provides a way to customize IMFContentDecryptionModuleFactory creation to
  // support different key systems and for testing.
  using CreateCdmFactoryCB = base::RepeatingCallback<HRESULT(
      Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& factory)>;
  void SetCreateCdmFactoryCallbackForTesting(
      const std::string& key_system,
      CreateCdmFactoryCB create_cdm_factory_cb);

  // CdmFactory implementation.
  void Create(const std::string& key_system,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) final;

 private:
  HRESULT GetCdmFactory(
      const std::string& key_system,
      Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& cdm_factory);
  HRESULT CreateCdmInternal(
      const std::string& key_system,
      const CdmConfig& cdm_config,
      Microsoft::WRL::ComPtr<IMFContentDecryptionModule>& mf_cdm);

  std::unique_ptr<CdmAuxiliaryHelper> helper_;
  base::FilePath user_data_dir_;

  // IMFContentDecryptionModule implementations typically require MTA to run.
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};

  // Key system to CreateCdmFactoryCB mapping.
  std::map<std::string, CreateCdmFactoryCB> create_cdm_factory_cbs_;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_FACTORY_H_
