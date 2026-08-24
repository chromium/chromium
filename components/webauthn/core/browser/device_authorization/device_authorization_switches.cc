// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/device_authorization/device_authorization_switches.h"

#include "base/command_line.h"
#include "url/gurl.h"

namespace webauthn {

GURL GetDeviceAuthorizationKeyEndpointUrl() {
  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kDeviceAuthorizationKeyEndpointUrlSwitch)) {
    const GURL url(command_line->GetSwitchValueASCII(
        kDeviceAuthorizationKeyEndpointUrlSwitch));
    if (url.is_valid()) {
      return url;
    }
  }
  return GURL(kDeviceAuthorizationKeyEndpointUrl);
}

}  // namespace webauthn
