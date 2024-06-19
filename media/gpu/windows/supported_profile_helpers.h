// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
#define MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_

#include <initguid.h>

#include "base/containers/flat_map.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/media_buildflags.h"
#include "ui/gfx/geometry/size.h"

namespace media {

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
// Vendor defined GUIDs for video decoder devices.
// Intel specific HEVC decoders. SCC decoders not added here.
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main_Intel,
            0x8c56eb1e,
            0x2b47,
            0x466f,
            0x8d,
            0x33,
            0x7d,
            0xbc,
            0xd6,
            0x3f,
            0x3d,
            0xf2);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main10_Intel,
            0x75fc75f7,
            0xc589,
            0x4a07,
            0xa2,
            0x5b,
            0x72,
            0xe0,
            0x3b,
            0x03,
            0x83,
            0xb3);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main12_Intel,
            0x8ff8a3aa,
            0xc456,
            0x4132,
            0xb6,
            0xef,
            0x69,
            0xd9,
            0xdd,
            0x72,
            0x57,
            0x1d);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main422_10_Intel,
            0xe484dcb8,
            0xcac9,
            0x4859,
            0x99,
            0xf5,
            0x5c,
            0x0d,
            0x45,
            0x06,
            0x90,
            0x89);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main422_12_Intel,
            0xc23dd857,
            0x874b,
            0x423c,
            0xb6,
            0xe0,
            0x82,
            0xce,
            0xaa,
            0x9b,
            0x11,
            0x8a);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main444_Intel,
            0x41a5af96,
            0xe415,
            0x4b0c,
            0x9d,
            0x03,
            0x90,
            0x78,
            0x58,
            0xe2,
            0x3e,
            0x78);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main444_10_Intel,
            0x6a6a81ba,
            0x912a,
            0x485d,
            0xb5,
            0x7f,
            0xcc,
            0xd2,
            0xd3,
            0x7b,
            0x8d,
            0x94);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main444_12_Intel,
            0x5b08e35d,
            0x0c66,
            0x4c51,
            0xa6,
            0xf1,
            0x89,
            0xd0,
            0x0c,
            0xb2,
            0xc1,
            0x97);

// Get the private GUID for HEVC range extension profile supported by Intel.
MEDIA_GPU_EXPORT GUID
GetHEVCRangeExtensionPrivateGUID(uint8_t bitdepth,
                                 VideoChromaSampling chroma_sampling);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

// Get the DXGI_FORMAT for the video decoder output texture, according to the
// bit depth and chroma sampling format.
MEDIA_GPU_EXPORT DXGI_FORMAT
GetOutputDXGIFormat(uint8_t bitdepth, VideoChromaSampling chroma_sampling);

struct SupportedResolutionRange {
  gfx::Size min_resolution;
  gfx::Size max_landscape_resolution;
  gfx::Size max_portrait_resolution;
};

using SupportedResolutionRangeMap =
    base::flat_map<VideoCodecProfile, SupportedResolutionRange>;

// Enumerates the extent of hardware decoding support for H.264, VP8, VP9, and
// AV1. If a codec is supported, its minimum and maximum supported resolutions
// are returned under the appropriate VideoCodecProfile entry.
//
// Notes:
// - VP8 and AV1 are only tested if their base::Feature entries are enabled.
// - Only baseline, main, and high H.264 profiles are supported.
MEDIA_GPU_EXPORT
SupportedResolutionRangeMap GetSupportedD3D11VideoDecoderResolutions(
    ComD3D11Device device,
    const gpu::GpuDriverBugWorkarounds& workarounds);

MEDIA_GPU_EXPORT
SupportedResolutionRangeMap GetSupportedD3D12VideoDecoderResolutions(
    ComD3D12Device device,
    const gpu::GpuDriverBugWorkarounds& workarounds);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
