// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_key_information.h"

#include <ostream>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace media {

CdmKeyInformation::CdmKeyInformation()
    : status(INTERNAL_ERROR), system_code(0) {}

CdmKeyInformation::CdmKeyInformation(const std::vector<uint8_t>& key_id,
                                     KeyStatus status,
                                     uint32_t system_code)
    : key_id(key_id), status(status), system_code(system_code) {}

CdmKeyInformation::CdmKeyInformation(const std::string& key_id,
                                     KeyStatus status,
                                     uint32_t system_code)
    : CdmKeyInformation(reinterpret_cast<const uint8_t*>(key_id.data()),
                        key_id.size(),
                        status,
                        system_code) {}

CdmKeyInformation::CdmKeyInformation(const uint8_t* key_id_data,
                                     size_t key_id_length,
                                     KeyStatus status,
                                     uint32_t system_code)
    : key_id(key_id_data, key_id_data + key_id_length),
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
