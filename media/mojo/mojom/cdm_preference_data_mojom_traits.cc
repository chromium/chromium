// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/cdm_preference_data_mojom_traits.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

// static
bool StructTraits<media::mojom::CdmPreferenceDataDataView,
                  std::unique_ptr<media::CdmPreferenceData>>::
    Read(media::mojom::CdmPreferenceDataDataView input,
         std::unique_ptr<media::CdmPreferenceData>* output) {
  base::UnguessableToken origin_id;
  if (!input.ReadOriginId(&origin_id))
    return false;

  absl::optional<std::vector<uint8_t>> client_token;
  if (!input.ReadClientToken(&client_token))
    return false;

  *output = std::make_unique<media::CdmPreferenceData>(origin_id, client_token);
  return true;
}

}  // namespace mojo
