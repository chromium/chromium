// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_HELPERS_H_
#define MEDIA_BASE_WIN_MF_HELPERS_H_

#include <mfapi.h>
#include <mfidl.h>
#include <stdint.h>
#include <wrl/client.h>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/media_buildflags.h"

struct ID3D11DeviceChild;
struct ID3D11Device;
class IMFMediaType;

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
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION uint8_t* buffer_;
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
// defined in ksmedia.h.
MEDIA_EXPORT ChannelLayout ChannelConfigToChannelLayout(ChannelConfig config);

// Converts a GUID (little endian) to a bytes array (big endian).
MEDIA_EXPORT std::vector<uint8_t> ByteArrayFromGUID(REFGUID guid);

// Returns a GUID from a binary serialization of a GUID string in network byte
// order format.
MEDIA_EXPORT GUID GetGUIDFromString(const std::string& guid_string);

// Returns a binary serialization of a GUID string in network byte order format.
MEDIA_EXPORT std::string GetStringFromGUID(REFGUID guid);

// Given an AudioDecoderConfig, get its corresponding IMFMediaType format.
// Note:
// IMFMediaType is derived from IMFAttributes and hence all the of information
// in a media type is store as attributes.
// https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
// has a list of media type attributes.
MEDIA_EXPORT HRESULT
GetDefaultAudioType(const AudioDecoderConfig decoder_config,
                    IMFMediaType** media_type_out);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Given an AudioDecoderConfig which represents AAC audio, get its
// corresponding IMFMediaType format (by calling GetDefaultAudioType)
// and populate the aac_extra_data in the decoder_config into the
// returned IMFMediaType.
MEDIA_EXPORT HRESULT GetAacAudioType(const AudioDecoderConfig& decoder_config,
                                     IMFMediaType** media_type_out);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
// Given an AudioDecoderConfig which represents AC4 audio, get its
// corresponding IMFMediaType format (by calling GetDefaultAudioType)
// and populate the AC4 extra_data in the decoder_config into the
// returned IMFMediaType.
MEDIA_EXPORT HRESULT GetAC4AudioType(const AudioDecoderConfig& decoder_config,
                                     IMFMediaType** media_type_out);
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

// A wrapper of SubsampleEntry for MediaFoundation. The data blob associated
// with MFSampleExtension_Encryption_SubSample_Mapping attribute should contain
// an array of byte ranges as DWORDs where every two DWORDs make a set.
// SubsampleEntry has a set of uint32_t that needs to be converted to DWORDs.
struct MediaFoundationSubsampleEntry {
  explicit MediaFoundationSubsampleEntry(SubsampleEntry entry)
      : clear_bytes(entry.clear_bytes), cipher_bytes(entry.cypher_bytes) {}
  MediaFoundationSubsampleEntry() = default;
  DWORD clear_bytes = 0;
  DWORD cipher_bytes = 0;
};

// Converts between MFTIME and TimeDelta. MFTIME defines units of 100
// nanoseconds. See
// https://learn.microsoft.com/en-us/windows/win32/medfound/mftime
MEDIA_EXPORT MFTIME TimeDeltaToMfTime(base::TimeDelta time);
MEDIA_EXPORT base::TimeDelta MfTimeToTimeDelta(MFTIME mf_time);

// Converts `codec` into a MediaFoundation subtype. `profile` must be provided
// when converting VideoCodec::kDolbyVision.
MEDIA_EXPORT GUID
VideoCodecToMFSubtype(VideoCodec codec,
                      VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN);

// Converts `video_pixel_format` into a MediaFoundation subtype.
MEDIA_EXPORT GUID
VideoPixelFormatToMFSubtype(VideoPixelFormat video_pixel_format);

// Converts `primaries` into an MFVideoPrimaries value
MEDIA_EXPORT MFVideoPrimaries
VideoPrimariesToMFVideoPrimaries(gfx::ColorSpace::PrimaryID primaries);

// Callback to transform a Media Foundation sample when converting from the
// DecoderBuffer if needed.
using TransformSampleCB =
    base::OnceCallback<HRESULT(Microsoft::WRL::ComPtr<IMFSample>& sample)>;

// Converts the DecoderBuffer back to a Media Foundation sample.
// `TransformSampleCB` is to allow derived classes to transform the Media
// Foundation sample if needed.
MEDIA_EXPORT HRESULT
GenerateSampleFromDecoderBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                                IMFSample** sample_out,
                                GUID* last_key_id,
                                TransformSampleCB transform_sample_cb);

// Creates a DecryptConfig from a Media Foundation sample.
MEDIA_EXPORT HRESULT
CreateDecryptConfigFromSample(IMFSample* mf_sample,
                              const GUID& key_id,
                              std::unique_ptr<DecryptConfig>* decrypt_config);

// Converts `frame` into an IMFSample, using an underlying D3D texture,
// reading back from the GPU, or copying the frame contents as necessary.
MEDIA_EXPORT HRESULT GenerateSampleFromVideoFrame(
    const media::VideoFrame* frame,
    DXGIDeviceManager* dxgi_device_manager,
    bool use_dxgi_buffer,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* staging_texture,
    DWORD buffer_alignment,
    IMFSample** sample_out);

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_HELPERS_H_
