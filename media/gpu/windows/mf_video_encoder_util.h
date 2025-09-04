// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_UTIL_H_
#define MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_UTIL_H_

#include <codecapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <stdint.h>
#include <wrl/client.h>

#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/media_buildflags.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct FramerateAndResolution {
  uint32_t frame_rate;
  gfx::Size resolution;
};

enum class DriverVendor { kOther, kNvidia, kIntel, kAMD, kQualcomm };

static constexpr size_t kDefaultFrameRateNumerator = 30;
static constexpr size_t kDefaultFrameRateDenominator = 1;

// The default supported max framerate and resolution.
static constexpr FramerateAndResolution kDefaultMaxFramerateAndResolution = {
    kDefaultFrameRateNumerator / kDefaultFrameRateDenominator,
    gfx::Size(1920, 1080)};

// The default supported min resolution.
static constexpr gfx::Size kDefaultMinResolution(32, 32);

// For H.264, some NVIDIA GPUs may report `MF_VIDEO_MAX_MB_PER_SEC` value equals
// to `6799902`, resulting chromium think 8K & 30fps is supported, and some
// Intel GPUs only support level 5.2. Since most devices only support up to 4K,
// so we set level 5.2 as the max allowed level here to limit max resolution and
// framerate combination can only go up to 2K & 172fps, or 4K & 64fps.
static constexpr FramerateAndResolution kLegacy2KMaxFramerateAndResolution = {
    172, gfx::Size(1920, 1080)};
static constexpr FramerateAndResolution kLegacy4KMaxFramerateAndResolution = {
    64, gfx::Size(3840, 2160)};

// Some Intel HMFTs report very high throughput via the encoder attribute
// MF_VIDEO_MAX_MB_PER_SEC. When translated to FPS at common resolutions
// (e.g., 1920x1080), this can imply >300 fps. However, empirical testing
// shows an effective upper bound of ~180 fps where stability and correctness
// are maintained. To avoid overcommitting the encoder and to keep behavior
// predictable across devices and drivers, we cap the VP9 maximum framerate at
// 180 fps for 2K/4K/8K.
static constexpr FramerateAndResolution kVP9Modern2KMaxFramerateAndResolution =
    {180, gfx::Size(1920, 1080)};
static constexpr FramerateAndResolution kVP9Modern4KMaxFramerateAndResolution =
    {180, gfx::Size(3840, 2160)};
static constexpr FramerateAndResolution kVP9Modern8KMaxFramerateAndResolution =
    {180, gfx::Size(7680, 4320)};

// For H.265/AV1, some NVIDIA GPUs may report `MF_VIDEO_MAX_MB_PER_SEC` value
// equals to `7255273`, resulting chromium think 2K & 880fps is supported. Since
// the max level of H.265/AV1 (6.2/6.3) do not allow framerate >= 300fps, so we
// set level 6.2/6.3 as the max allowed level here and limit max resolution and
// framerate combination can only go up to 2K/4K & 300fps, 8K & 128fps.
static constexpr FramerateAndResolution kModern2KMaxFramerateAndResolution = {
    300, gfx::Size(1920, 1080)};
static constexpr FramerateAndResolution kModern4KMaxFramerateAndResolution = {
    300, gfx::Size(3840, 2160)};
static constexpr FramerateAndResolution kModern8KMaxFramerateAndResolution = {
    128, gfx::Size(7680, 4320)};

static constexpr CLSID kIntelAV1HybridEncoderCLSID = {
    0x62c053ce,
    0x5357,
    0x4794,
    {0x8c, 0x5a, 0xfb, 0xef, 0xfe, 0xff, 0xb8, 0x2d}};

static constexpr auto kSupportedPixelFormats =
    base::MakeFixedFlatSet<VideoPixelFormat>(
        {PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12});
static constexpr auto kSupportedPixelFormatsD3DVideoProcessing =
    base::MakeFixedFlatSet<VideoPixelFormat>(
        {PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12, PIXEL_FORMAT_YV12,
         PIXEL_FORMAT_NV21, PIXEL_FORMAT_ARGB, PIXEL_FORMAT_XRGB,
         PIXEL_FORMAT_ABGR, PIXEL_FORMAT_XBGR});

static constexpr int kH26xMaxQp = 51;
static constexpr uint64_t kVP9MaxQIndex = 255;
static constexpr uint64_t kAV1MaxQIndex = 255;

// Quantizer parameter used in libvpx vp9 rate control, whose range is 0-63.
// These are based on WebRTC's defaults, cite from
// third_party/webrtc/media/engine/webrtc_video_engine.h.
static constexpr uint8_t kVP9MinQuantizer = 2;
static constexpr uint8_t kVP9MaxQuantizer = 56;
// Default value from
// //third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc,
static constexpr uint8_t kAV1MinQuantizer = 10;
// //third_party/webrtc/media/engine/webrtc_video_engine.h.
static constexpr uint8_t kAV1MaxQuantizer = 56;

// The range for the quantization parameter is determined by examining the
// estimated QP values from the SW bitrate controller in various encoding
// scenarios.
static constexpr uint8_t kH264MinQuantizer = 16;
static constexpr uint8_t kH264MaxQuantizer = 51;

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
// For H.265, ideally we may reuse Min/MaxQp for H.264 from
// media/gpu/vaapi/h264_vaapi_video_encoder_delegate.cc. However
// test shows most of the drivers require a min QP of 10 to reach
// target bitrate especially at low resolution.
static constexpr uint8_t kH265MinQuantizer = 10;
static constexpr uint8_t kH265MaxQuantizer = 42;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

// Converts AV1/VP9 qindex (0-255) to the quantizer parameter input in MF
// AVEncVideoEncodeQP.
uint8_t QindextoAVEncQP(VideoCodec codec, uint8_t q_index);

// Converts AV1/VP9 AVEncVideoEncodeQP values to qindex (0-255) range.
uint8_t AVEncQPtoQindex(VideoCodec codec, uint8_t avenc_qp);

// Returns true if |qp| is a valid quantizer parameter for |codec|.
bool IsValidQp(VideoCodec codec, uint64_t qp);

// Returns the maximum quantizer value for |codec|.
uint8_t GetMaxQuantizer(VideoCodec codec);

// Converts VideoCodecProfile to eAVEncH264VProfile.
eAVEncH264VProfile GetH264VProfile(VideoCodecProfile profile,
                                   bool is_constrained_h264);

// Converts VideoCodecProfile to eAVEncVP9VProfile.
eAVEncVP9VProfile GetVP9VProfile(VideoCodecProfile profile);

// Converts VideoCodecProfile to eAVEncAV1VProfile.
eAVEncH265VProfile GetHEVCProfile(VideoCodecProfile profile);

// Returns the driver vendor of the given |encoder|.
DriverVendor GetDriverVendor(IMFActivate* encoder);

// Get the maximum number of temporal layers supported by the given |encoder|
// from |vendor|, taking into account |workarounds|.
int GetMaxTemporalLayerVendorLimit(
    DriverVendor vendor,
    VideoCodec codec,
    const gpu::GpuDriverBugWorkarounds& workarounds);

// Enumeration of HMFT without specifying LUID.
std::vector<Microsoft::WRL::ComPtr<IMFActivate>>
EnumerateHardwareEncodersLegacy(VideoCodec codec);

// Adapter-based enumeration of HMFT. The adapter with lower index is enumerated
// first.
std::vector<Microsoft::WRL::ComPtr<IMFActivate>> EnumerateHardwareEncoders(
    VideoCodec codec);

// Get the maximum supported framerate and resolution combinations for the given
// codec and HMFT. If |allow_set_output_type| is true, it will try to set the
// output type to get the maximum supported framerate and resolution; otherwise,
// we merely query the maximum supported macroblocks per second for calculation.
std::vector<FramerateAndResolution> GetMaxFramerateAndResolutionsFromMFT(
    VideoCodec codec,
    IMFTransform* encoder,
    bool allow_set_output_type);

// Get the minimum supported encoding resolution for the given codec and vendor.
gfx::Size GetMinResolution(VideoCodec codec, DriverVendor vendor);

// Get the hash of the CLSID of the IMFActivate instance.
size_t GetMFTGuidHash(IMFActivate* activate);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_UTIL_H_
