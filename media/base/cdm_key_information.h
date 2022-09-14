// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_KEY_INFORMATION_H_
#define MEDIA_BASE_CDM_KEY_INFORMATION_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT CdmKeyInformation {
  enum KeyStatus {
    USABLE = 0,
    INTERNAL_ERROR = 1,
    EXPIRED = 2,
    OUTPUT_RESTRICTED = 3,
    OUTPUT_DOWNSCALED = 4,
    KEY_STATUS_PENDING = 5,
    RELEASED = 6,
    KEY_STATUS_MAX = RELEASED
  };

  // Default constructor needed for passing this type through IPC. Regular
  // code should use one of the other constructors.
  CdmKeyInformation();
  CdmKeyInformation(const std::vector<uint8_t>& key_id,
                    KeyStatus status,
                    uint32_t system_code);
  CdmKeyInformation(const std::string& key_id,
                    KeyStatus status,
                    uint32_t system_code);
  CdmKeyInformation(const uint8_t* key_id_data,
                    size_t key_id_length,
                    KeyStatus status,
                    uint32_t system_code);
  CdmKeyInformation(const CdmKeyInformation& other);
  ~CdmKeyInformation();

  static std::string KeyStatusToString(KeyStatus key_status);

  std::vector<uint8_t> key_id;
  KeyStatus status;
  uint32_t system_code;
};

// The following are for logging use only.

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      CdmKeyInformation::KeyStatus status);

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      const CdmKeyInformation& info);

}  // namespace media

#endif  // MEDIA_BASE_CDM_KEY_INFORMATION_H_
