// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_key_information.h"

#include <ostream>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace media {

CdmKeyInformation::CdmKeyInformation()
    : status(INTERNAL_ERROR), system_code(0) {}

CdmKeyInformation::CdmKeyInformation(const std::string& key_id,
                                     KeyStatus status,
                                     uint32_t system_code)
    : CdmKeyInformation(base::as_byte_span(key_id), status, system_code) {}

CdmKeyInformation::CdmKeyInformation(const uint8_t* key_id_data,
                                     size_t key_id_length,
                                     KeyStatus status,
                                     uint32_t system_code)
    // UNSAFE_TODO: it would be nice to remove this four-arg constructor.
    // We can't immediately do so because it requires a more intrusive
    // refactor of callsites where the `key_id_data` is held as a
    // pointer-plus-length (e.g. `cdm::KeyInformation`).
    : UNSAFE_TODO(key_id(key_id_data, key_id_data + key_id_length)),
      status(status),
      system_code(system_code) {}

CdmKeyInformation::CdmKeyInformation(base::span<const uint8_t> key_id_data,
                                     KeyStatus status,
                                     uint32_t system_code)
    : key_id(base::ToVector(key_id_data)),
      status(status),
      system_code(system_code) {}

CdmKeyInformation::CdmKeyInformation(const CdmKeyInformation& other) = default;

CdmKeyInformation::~CdmKeyInformation() = default;

// static
std::string CdmKeyInformation::KeyStatusToString(KeyStatus key_status) {
  switch (key_status) {
    case USABLE:
      return "USABLE";
    case INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case EXPIRED:
      return "EXPIRED";
    case OUTPUT_RESTRICTED:
      return "OUTPUT_RESTRICTED";
    case OUTPUT_DOWNSCALED:
      return "OUTPUT_DOWNSCALED";
    case KEY_STATUS_PENDING:
      return "KEY_STATUS_PENDING";
    case RELEASED:
      return "RELEASED";
    case USABLE_IN_FUTURE:
      return "USABLE_IN_FUTURE";
  }

  NOTREACHED();
}

std::ostream& operator<<(std::ostream& os,
                         CdmKeyInformation::KeyStatus status) {
  return os << CdmKeyInformation::KeyStatusToString(status);
}

std::ostream& operator<<(std::ostream& os, const CdmKeyInformation& info) {
  return os << "key_id = " << base::HexEncode(info.key_id)
            << ", status = " << info.status
            << ", system_code = " << info.system_code;
}

}  // namespace media
