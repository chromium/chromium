// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_HELPERS_H_
#define MEDIA_BASE_WIN_MF_HELPERS_H_

#include <mfapi.h>
#include <stdint.h>
#include <wrl/client.h>

#include "base/logging.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

struct ID3D11DeviceChild;
struct ID3D11Device;

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
MEDIA_EXPORT Microsoft::WRL::ComPtr<IMFSample> CreateEmptySampleWithBuffer(
    uint32_t buffer_length,
    int align);

// Provides scoped access to the underlying buffer in an IMFMediaBuffer
// instance.
class MEDIA_EXPORT MediaBufferScopedPointer {
 public:
  explicit MediaBufferScopedPointer(IMFMediaBuffer* media_buffer);

  MediaBufferScopedPointer(const MediaBufferScopedPointer&) = delete;
  MediaBufferScopedPointer& operator=(const MediaBufferScopedPointer&) = delete;

  ~MediaBufferScopedPointer();

  uint8_t* get() { return buffer_; }
  DWORD current_length() const { return current_length_; }
  DWORD max_length() const { return max_length_; }

 private:
  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer_;
  uint8_t* buffer_;
  DWORD max_length_;
  DWORD current_length_;
};

// Copies |in_string| to |out_string| that is allocated with CoTaskMemAlloc().
MEDIA_EXPORT HRESULT CopyCoTaskMemWideString(LPCWSTR in_string,
                                             LPWSTR* out_string);

// Set the debug name of a D3D11 resource for use with ETW debugging tools.
// D3D11 retains the string passed to this function.
MEDIA_EXPORT HRESULT SetDebugName(ID3D11DeviceChild* d3d11_device_child,
                                  const char* debug_string);
MEDIA_EXPORT HRESULT SetDebugName(ID3D11Device* d3d11_device,
                                  const char* debug_string);

// Represents audio channel configuration constants as understood by Windows.
// E.g. KSAUDIO_SPEAKER_MONO.  For a list of possible values see:
// http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx
using ChannelConfig = uint32_t;

// Converts Microsoft's channel configuration to ChannelLayout.
// This mapping is not perfect but the best we can do given the current
// ChannelLayout enumerator and the Windows-specific speaker configurations
// defined in ksmedia.h. Don't assume that the channel ordering in
// ChannelLayout is exactly the same as the Windows specific configuration.
// As an example: KSAUDIO_SPEAKER_7POINT1_SURROUND is mapped to
// CHANNEL_LAYOUT_7_1 but the positions of Back L, Back R and Side L, Side R
// speakers are different in these two definitions.
MEDIA_EXPORT ChannelLayout ChannelConfigToChannelLayout(ChannelConfig config);

// Converts a GUID (little endian) to a bytes array (big endian).
MEDIA_EXPORT std::vector<uint8_t> ByteArrayFromGUID(REFGUID guid);

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_HELPERS_H_
