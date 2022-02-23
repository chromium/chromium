// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_status.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "media/base/media_serializers.h"

namespace media {

D3D11Status HresultToStatus(HRESULT hresult,
                            D3D11Status::Codes code,
                            const base::Location& location) {
  if (SUCCEEDED(hresult))
    return D3D11Status::Codes::kOk;

  std::string sys_err = logging::SystemErrorCodeToString(hresult);
  if (!base::IsStringUTF8AllowingNoncharacters(sys_err)) {
    std::stringstream hresult_str_repr;
    hresult_str_repr << std::hex << hresult;
    sys_err = hresult_str_repr.str();
  }

  return D3D11Status(code, sys_err, location)
      .WithData("hresult", static_cast<uint32_t>(hresult));
}

}  // namespace media
