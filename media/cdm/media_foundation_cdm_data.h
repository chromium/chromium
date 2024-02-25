// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_MEDIA_FOUNDATION_CDM_DATA_H_
#define MEDIA_CDM_MEDIA_FOUNDATION_CDM_DATA_H_

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace media {
struct MEDIA_EXPORT MediaFoundationCdmData {
  MediaFoundationCdmData();
  MediaFoundationCdmData(
      const base::UnguessableToken& origin_id,
      const std::optional<std::vector<uint8_t>>& client_token,
      const base::FilePath& cdm_store_path_root);

  MediaFoundationCdmData(const MediaFoundationCdmData& other) = delete;
  MediaFoundationCdmData& operator=(const MediaFoundationCdmData& other) =
      delete;

  ~MediaFoundationCdmData();

  base::UnguessableToken origin_id;
  std::optional<std::vector<uint8_t>> client_token;
  base::FilePath cdm_store_path_root;
};
}  // namespace media

#endif  // MEDIA_CDM_MEDIA_FOUNDATION_CDM_DATA_H_
