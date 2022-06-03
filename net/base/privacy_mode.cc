// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/privacy_mode.h"

#include "base/notreached.h"

namespace net {

const char* PrivacyModeToDebugString(PrivacyMode privacy_mode) {
  switch (privacy_mode) {
    case PRIVACY_MODE_DISABLED:
      return "disabled";
    case PRIVACY_MODE_ENABLED:
      return "enabled";
    case PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS:
      return "enabled without client certs";
  }
  NOTREACHED();
  return "";
}

}  // namespace net
