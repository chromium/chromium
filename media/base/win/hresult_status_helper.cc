// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/hresult_status_helper.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace media {

D3D11Status HresultToStatus(HRESULT hresult,
                            D3D11Status::Codes code,
                            const char* message,
                            const base::Location& location) {
  if (SUCCEEDED(hresult))
    return D3D11Status::Codes::kOk;

  std::string sys_err = logging::SystemErrorCodeToString(hresult);
  if (!base::IsStringUTF8AllowingNoncharacters(sys_err))
    sys_err = "System error string is invalid";

  return D3D11Status(code, message == nullptr ? "HRESULT" : message, location)
      .WithData("value", sys_err);
}

}  // namespace media
