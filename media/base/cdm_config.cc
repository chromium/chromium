// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_config.h"

#include <ostream>

namespace media {

std::ostream& operator<<(std::ostream& os, const CdmConfig& cdm_config) {
  return os << "{key_system=" << cdm_config.key_system
            << ", allow_distinctive_identifier="
            << cdm_config.allow_distinctive_identifier
            << ", allow_persistent_state=" << cdm_config.allow_persistent_state
            << ", use_hw_secure_codecs=" << cdm_config.use_hw_secure_codecs
            << "}";
}

}  // namespace media
