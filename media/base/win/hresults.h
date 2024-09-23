// Copyright 2022 The Chromium Authors
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

inline constexpr HRESULT kErrorInitializeMediaFoundation =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA01);  // 0x8004FA01
inline constexpr HRESULT kErrorInitializeVideoWindowClass =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA02);  // 0x8004FA02
inline constexpr HRESULT kErrorCdmProxyReceivedInInvalidState =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA03);  // 0x8004FA03
inline constexpr HRESULT kErrorResolveCoreWinRTStringDelayload =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA04);  // 0x8004FA04
inline constexpr HRESULT kErrorZeroProtectionSystemId =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA05);  // 0x8004FA05
inline constexpr HRESULT kErrorLoadLibrary =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA06);  // 0x8004FA06
inline constexpr HRESULT kErrorGetFunctionPointer =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA07);  // 0x8004FA07
inline constexpr HRESULT kErrorInvalidCdmProxy =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFA08);  // 0x8004FA08

// HRESULTs defined by Windows but are not exposed in public header files.

// HRESULT 0x8004CD12 is DRM_E_TEE_INVALID_HWDRM_STATE, which can happen
// during OS sleep/resume, or moving video to different graphics adapters.
// When it happens, the HWDRM state is reset.
inline constexpr HRESULT DRM_E_TEE_INVALID_HWDRM_STATE =
    static_cast<HRESULT>(0x8004CD12);

}  // namespace media

#endif  // MEDIA_BASE_WIN_HRESULTS_H_
