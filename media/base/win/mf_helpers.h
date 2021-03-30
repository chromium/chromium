// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_HELPERS_H_
#define MEDIA_BASE_WIN_MF_HELPERS_H_

#include <mfapi.h>
#include <stdint.h>
#include <wrl/client.h>

#include "base/logging.h"
#include "base/macros.h"
#include "media/base/win/mf_initializer_export.h"

struct ID3D11DeviceChild;

namespace media {

// Helper function to print HRESULT to std::string.
const auto PrintHr = logging::SystemErrorCodeToString;

// Helper macro for DVLOG with function name and this pointer.
#define DVLOG_FUNC(level) DVLOG(level) << __func__ << ": (" << this << ") "

// Macros that contain return statements can make code harder to read. Only use
// these when necessary, e.g. in places where we deal with a lot of Windows API
// calls, for each of which we have to check the returned HRESULT.
// See discussion thread at:
// https://groups.google.com/a/chromium.org/d/msg/cxx/zw5Xmcs--S4/r7Fwb-TsCAAJ

#define RETURN_IF_FAILED(expr)                                          \
  do {                                                                  \
    HRESULT hresult = (expr);                                           \
    if (FAILED(hresult)) {                                              \
      DLOG(ERROR) << __func__ << ": failed with \"" << PrintHr(hresult) \
                  << "\"";                                              \
      return hresult;                                                   \
    }                                                                   \
  } while (0)

#define RETURN_ON_FAILURE(success, log, ret) \
  do {                                       \
    if (!(success)) {                        \
      DLOG(ERROR) << log;                    \
      return ret;                            \
    }                                        \
  } while (0)

#define RETURN_ON_HR_FAILURE(hresult, log, ret) \
  RETURN_ON_FAILURE(SUCCEEDED(hresult), log << ", " << PrintHr(hresult), ret);

// Creates a Media Foundation sample with one buffer of length |buffer_length|
// on a |align|-byte boundary. Alignment must be a perfect power of 2 or 0.
MF_INITIALIZER_EXPORT Microsoft::WRL::ComPtr<IMFSample>
CreateEmptySampleWithBuffer(uint32_t buffer_length, int align);

// Provides scoped access to the underlying buffer in an IMFMediaBuffer
// instance.
class MF_INITIALIZER_EXPORT MediaBufferScopedPointer {
 public:
  explicit MediaBufferScopedPointer(IMFMediaBuffer* media_buffer);
  ~MediaBufferScopedPointer();

  uint8_t* get() { return buffer_; }
  DWORD current_length() const { return current_length_; }
  DWORD max_length() const { return max_length_; }

 private:
  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer_;
  uint8_t* buffer_;
  DWORD max_length_;
  DWORD current_length_;

  DISALLOW_COPY_AND_ASSIGN(MediaBufferScopedPointer);
};

// Copies |in_string| to |out_string| that is allocated with CoTaskMemAlloc().
MF_INITIALIZER_EXPORT HRESULT CopyCoTaskMemWideString(LPCWSTR in_string,
                                                      LPWSTR* out_string);

// Set the debug name of a D3D11 resource for use with ETW debugging tools.
// D3D11 retains the string passed to this function.
MF_INITIALIZER_EXPORT HRESULT
SetDebugName(ID3D11DeviceChild* d3d11_device_child, const char* debug_string);
}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_HELPERS_H_
