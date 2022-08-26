// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_HRESULTS_H_
#define MEDIA_BASE_WIN_HRESULTS_H_

#include <winerror.h>

namespace media {

// Custom error HRESULTs used by Chromium media code on Windows.
// See https://docs.microsoft.com/en-us/windows/win32/com/codes-in-facility-itf
// Chromium media code is reserving the range [0x8004FA00, 0x8004FBFF].
// Reported to metrics, please never modify or reuse existing values.
// For new values, please also add them to enums.xml.

constexpr HRESULT kErrorInitializeMediaFoundation =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA01);
constexpr HRESULT kErrorInitializeVideoWindowClass =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA02);
constexpr HRESULT kErrorCdmProxyReceivedInInvalidState =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA03);

}  // namespace media

#endif  // MEDIA_BASE_WIN_HRESULTS_H_
