// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_UTILS_H_
#define MEDIA_GPU_V4L2_V4L2_UTILS_H_

#include <string>

#include <linux/videodev2.h>

#include "base/functional/callback.h"
#include "media/base/video_codecs.h"

namespace gfx {
class Size;
}
namespace media {

using IoctlAsCallback = base::RepeatingCallback<int(int, void*)>;

// Returns a human readable description of |memory|.
const char* V4L2MemoryToString(v4l2_memory memory);

// Returns a human readable description of |format|.
std::string V4L2FormatToString(const struct v4l2_format& format);

// Returns a human readable description of |buffer|
std::string V4L2BufferToString(const struct v4l2_buffer& buffer);

// Translates |v4l2_codec| (a Control ID, e.g. V4L2_CID_MPEG_VIDEO_VP8_PROFILE)
// and |v4l2_profile| (e.g. V4L2_MPEG_VIDEO_VP8_PROFILE_0) to a
// media::VideoCodecProfile, if those are supported by Chrome. It returns
// VIDEO_CODEC_PROFILE_UNKNOWN otherwise.
VideoCodecProfile V4L2ProfileToVideoCodecProfile(uint32_t v4l2_codec,
                                                 uint32_t v4l2_profile);

// Enumerates the supported VideoCodecProfiles for a given device (accessed via
// |ioctl_cb|) and for |codec_as_pix_fmt| (e.g. V4L2_PIX_FMT_VP9). Returns an
// empty vector if |codec_as_pix_fmt| is not supported by Chrome, or the
// associated profiles cannot be enumerated or they are all unsupported
// themselves. Notably, if the device driver doesn't support enumeration of a
// supported |codec_as_pix_fmt| (i.e. VIDIOC_QUERYCTRL), a default list of
// profiles is returned (this happens for example for VP8 on Hana MTK8173, or
// for HEVC on Trogdor QC SC7180).
std::vector<VideoCodecProfile> EnumerateSupportedProfilesForV4L2Codec(
    const IoctlAsCallback& ioctl_cb,
    uint32_t codec_as_pix_fmt);

// Enumerates all supported pixel formats for a given device (accessed via
// |ioctl_cb|) and for |buf_type|; these will be the supported video codecs
// (e.g. V4L2_PIX_FMT_VP9) for V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE.
std::vector<uint32_t> EnumerateSupportedPixFmts(const IoctlAsCallback& ioctl_cb,
                                                v4l2_buf_type buf_type);

// Gets minimum and maximum resolution for fourcc |pixelformat|. If the driver
// doesn't support enumeration, default values are returned instead.
void GetSupportedResolution(const IoctlAsCallback& ioctl_cb,
                            uint32_t pixelformat,
                            gfx::Size* min_resolution,
                            gfx::Size* max_resolution);

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_UTILS_H_
