// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/media_foundation_cdm_data_mojom_traits.h"

#include <optional>

namespace mojo {

// static
bool StructTraits<media::mojom::MediaFoundationCdmDataDataView,
                  std::unique_ptr<media::MediaFoundationCdmData>>::
    Read(media::mojom::MediaFoundationCdmDataDataView input,
         std::unique_ptr<media::MediaFoundationCdmData>* output) {
  base::UnguessableToken origin_id;
  if (!input.ReadOriginId(&origin_id))
    return false;

  std::optional<std::vector<uint8_t>> client_token;
  if (!input.ReadClientToken(&client_token))
    return false;

  base::FilePath cdm_store_path_root;
  if (!input.ReadCdmStorePathRoot(&cdm_store_path_root))
    return false;

  *output = std::make_unique<media::MediaFoundationCdmData>(
      origin_id, std::move(client_token), std::move(cdm_store_path_root));
  return true;
}

}  // namespace mojo
