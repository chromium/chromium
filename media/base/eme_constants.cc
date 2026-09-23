// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/eme_constants.h"

#include "base/notreached.h"

namespace media {

const char* EmeInitDataTypeToString(EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case EmeInitDataType::WEBM:
      return "webm";
    case EmeInitDataType::CENC:
      return "cenc";
    case EmeInitDataType::KEYIDS:
      return "keyids";
    case EmeInitDataType::UNKNOWN:
      return "unknown";
  }

  NOTREACHED();
}

EmeInitDataType StringToEmeInitDataType(std::string_view init_data_type) {
  if (init_data_type == "cenc") {
    return EmeInitDataType::CENC;
  }
  if (init_data_type == "keyids") {
    return EmeInitDataType::KEYIDS;
  }
  if (init_data_type == "webm") {
    return EmeInitDataType::WEBM;
  }

  return EmeInitDataType::UNKNOWN;
}

}  // namespace media
