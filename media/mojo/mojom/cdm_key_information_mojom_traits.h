// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_

#include "media/base/cdm_key_information.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::CdmKeyStatus,
                  media::CdmKeyInformation::KeyStatus> {
  static media::mojom::CdmKeyStatus ToMojom(
      media::CdmKeyInformation::KeyStatus key_status);

  static bool FromMojom(media::mojom::CdmKeyStatus input,
                        media::CdmKeyInformation::KeyStatus* out);
};

template <>
struct StructTraits<media::mojom::CdmKeyInformationDataView,
                    std::unique_ptr<media::CdmKeyInformation>> {
  static const std::vector<uint8_t>& key_id(
      const std::unique_ptr<media::CdmKeyInformation>& input) {
    return input->key_id;
  }

  static media::CdmKeyInformation::KeyStatus status(
      const std::unique_ptr<media::CdmKeyInformation>& input) {
    return input->status;
  }

  static uint32_t system_code(
      const std::unique_ptr<media::CdmKeyInformation>& input) {
    return input->system_code;
  }

  static bool Read(media::mojom::CdmKeyInformationDataView input,
                   std::unique_ptr<media::CdmKeyInformation>* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_
