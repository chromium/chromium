// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_CDM_PREFERENCE_DATA_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_CDM_PREFERENCE_DATA_MOJOM_TRAITS_H_

#include <vector>

#include "base/unguessable_token.h"
#include "media/cdm/cdm_preference_data.h"
#include "media/mojo/mojom/cdm_document_service.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::CdmPreferenceDataDataView,
                    std::unique_ptr<media::CdmPreferenceData>> {
  static base::UnguessableToken origin_id(
      const std::unique_ptr<media::CdmPreferenceData>& input) {
    return input->origin_id;
  }

  static absl::optional<std::vector<uint8_t>> client_token(
      const std::unique_ptr<media::CdmPreferenceData>& input) {
    return input->client_token;
  }

  static bool Read(media::mojom::CdmPreferenceDataDataView input,
                   std::unique_ptr<media::CdmPreferenceData>* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_CDM_PREFERENCE_DATA_MOJOM_TRAITS_H_
