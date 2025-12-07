// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_UTIL_H_
#define MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_UTIL_H_

#include <mfcontentdecryptionmodule.h>
#include <stdint.h>
#include <wrl/client.h>

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "media/base/cdm_config.h"
#include "media/base/media_export.h"
#include "media/base/win/hresults.h"

namespace media {

using IsTypeSupportedValueOrError = base::expected<bool, HRESULT>;

MEDIA_EXPORT HRESULT CreateMediaFoundationCdm(
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const CdmConfig& cdm_config,
    const base::UnguessableToken& cdm_origin_id,
    const std::optional<std::vector<uint8_t>>& cdm_client_token,
    const base::FilePath& cdm_store_path_root,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModule>& mf_cdm);

MEDIA_EXPORT IsTypeSupportedValueOrError IsMediaFoundationContentTypeSupported(
    Microsoft::WRL::ComPtr<IMFExtendedDRMTypeSupport> mf_type_support,
    const std::string& key_system,
    const std::string& content_type);

}  // namespace media

#endif  // MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_UTIL_H_
