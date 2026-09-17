// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/media_foundation_cdm_data.h"

namespace media {

MediaFoundationCdmData::MediaFoundationCdmData() = default;

MediaFoundationCdmData::MediaFoundationCdmData(
    const base::UnguessableToken& origin_id,
    const base::FilePath& cdm_store_path_root)
    : origin_id(origin_id), cdm_store_path_root(cdm_store_path_root) {}

MediaFoundationCdmData::~MediaFoundationCdmData() = default;

}  // namespace media
