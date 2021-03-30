// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/hresult_status_helper.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace media {

Status HresultToStatus(HRESULT hresult,
                       const char* message,
                       StatusCode code,
                       const base::Location& location) {
  if (SUCCEEDED(hresult))
    return OkStatus();

  std::string sys_err = logging::SystemErrorCodeToString(hresult);
  if (!base::IsStringUTF8AllowingNoncharacters(sys_err))
    sys_err = "System error string is invalid";

  return Status(code, message == nullptr ? "HRESULT" : message, location)
      .WithData("value", sys_err);
}

}  // namespace media
