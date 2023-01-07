// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/sct_status_flags.h"

namespace net::ct {

bool IsValidSCTStatus(uint32_t status) {
  switch (status) {
    case net::ct::SCT_STATUS_LOG_UNKNOWN:
    case net::ct::SCT_STATUS_INVALID_SIGNATURE:
    case net::ct::SCT_STATUS_OK:
    case net::ct::SCT_STATUS_INVALID_TIMESTAMP:
      return true;
    case net::ct::SCT_STATUS_NONE:
      return false;
  }

  return false;
}

}  // namespace net::ct
