// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_CDM_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_CDM_CONFIG_MOJOM_TRAITS_H_

#include "media/base/cdm_config.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::CdmConfigDataView, media::CdmConfig> {
  static const std::string& key_system(const media::CdmConfig& input) {
    return input.key_system;
  }

  static bool allow_distinctive_identifier(const media::CdmConfig& input) {
    return input.allow_distinctive_identifier;
  }

  static bool allow_persistent_state(const media::CdmConfig& input) {
    return input.allow_persistent_state;
  }

  static bool use_hw_secure_codecs(const media::CdmConfig& input) {
    return input.use_hw_secure_codecs;
  }

  static bool Read(media::mojom::CdmConfigDataView input,
                   media::CdmConfig* output) {
    output->allow_distinctive_identifier = input.allow_distinctive_identifier();
    output->allow_persistent_state = input.allow_persistent_state();
    output->use_hw_secure_codecs = input.use_hw_secure_codecs();

    return input.ReadKeySystem(&output->key_system);
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_CDM_CONFIG_MOJOM_TRAITS_H_
