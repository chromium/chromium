// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_CDM_DATA_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_CDM_DATA_MOJOM_TRAITS_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "media/cdm/media_foundation_cdm_data.h"
#include "media/mojo/mojom/cdm_document_service.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::MediaFoundationCdmDataDataView,
                    std::unique_ptr<media::MediaFoundationCdmData>> {
  static const base::UnguessableToken& origin_id(
      const std::unique_ptr<media::MediaFoundationCdmData>& input) {
    return input->origin_id;
  }

  static const std::optional<std::vector<uint8_t>>& client_token(
      const std::unique_ptr<media::MediaFoundationCdmData>& input) {
    return input->client_token;
  }

  static const base::FilePath& cdm_store_path_root(
      const std::unique_ptr<media::MediaFoundationCdmData>& input) {
    return input->cdm_store_path_root;
  }

  static bool Read(media::mojom::MediaFoundationCdmDataDataView input,
                   std::unique_ptr<media::MediaFoundationCdmData>* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_CDM_DATA_MOJOM_TRAITS_H_
