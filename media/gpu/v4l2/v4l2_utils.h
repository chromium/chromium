// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_UTILS_H_
#define MEDIA_GPU_V4L2_V4L2_UTILS_H_

#include <linux/videodev2.h>
#include <sys/mman.h>

#include <optional>
#include <string>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "mojo/public/cpp/platform/platform_handle.h"

#ifndef V4L2_PIX_FMT_QC08C
#define V4L2_PIX_FMT_QC08C \
  v4l2_fourcc('Q', '0', '8', 'C') /* Qualcomm 8-bit compressed */
#endif

#ifndef V4L2_PIX_FMT_INVALID
#define V4L2_PIX_FMT_INVALID v4l2_fourcc('0', '0', '0', '0')
#endif

namespace gfx {
class Size;
}
namespace media {
class VideoFrameLayout;

using IoctlAsCallback = base::RepeatingCallback<int(int, void*)>;

// Ideally this should be a decltype(mmap) (void *mmap(void *addr, size_t
// length, int prot, int flags, int fd, off_t offset)), but the types of e.g.
// V4L2Device::Mmap are wrong.
// TODO(b/279980150): correct types and argument order and use decltype.
using MmapAsCallback =
    base::RepeatingCallback<void*(void*, unsigned int, int, int, unsigned int)>;

// This is the callback invoked after successfully allocating a secure buffer.
// Invocation of this is guaranteed to pass a valid FD w/ the corresponding
// secure handle.
using SecureBufferAllocatedCB =
    base::OnceCallback<void(base::ScopedFD fd, uint64_t secure_handle)>;

using AllocateSecureBufferAsCallback =
    base::RepeatingCallback<void(uint32_t size,
                                 SecureBufferAllocatedCB callback)>;

// Numerical value of ioctl() OK return value;
constexpr int kIoctlOk = 0;

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "MediaIoctlRequests" in src/tools/metrics/histograms/enums.xml.
enum class MediaIoctlRequests {
  kMediaIocDeviceInfo = 0,
  kMediaIocRequestAlloc = 1,
  kMediaRequestIocQueue = 2,
  kMediaRequestIocReinit = 3,
  kMaxValue = kMediaRequestIocReinit,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "VidiocIoctlRequests" in src/tools/metrics/histograms/enums.xml.
enum class VidiocIoctlRequests {
  kVidiocGFmt = 0,
  kVidiocSFmt = 1,
  kVidiocGSelection = 2,
  kVidiocExpbuf = 3,
  kVidiocReqbufs = 4,
  kVidiocQuerybuf = 5,
  kVidiocQbuf = 6,
  kVidiocDqbuf = 7,
  kVidiocStreamon = 8,
  kVidiocStreamoff = 9,
  kVidiocSExtCtrls = 10,
  kMaxValue = kVidiocSExtCtrls,
};

// Records Media.V4L2VideoDecoder.MediaIoctlError UMA when errors happen with
// media controller API ioctl requests.
void RecordMediaIoctlUMA(MediaIoctlRequests function);

// Records Vidioc.V4L2VideoDecoder.VidiocIoctlError UMA when errors happen with
// V4L2 API ioctl requests.
void RecordVidiocIoctlErrorUMA(VidiocIoctlRequests function);

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

// Returns number of planes of |pix_fmt|, or 1, if this is unknown.
size_t GetNumPlanesOfV4L2PixFmt(uint32_t pix_fmt);

// Composes VideoFrameLayout based on v4l2_format.
// If error occurs, it returns std::nullopt.
std::optional<VideoFrameLayout> V4L2FormatToVideoFrameLayout(
    const struct v4l2_format& format);

// Query the driver to see what scalability modes are supported for the driver.
std::vector<SVCScalabilityMode> GetSupportedScalabilityModesForV4L2Codec(
    const IoctlAsCallback& ioctl_cb,
    VideoCodecProfile media_profile);

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

// Translates a media::VideoCodecProfile to a supported pixel format
// (e.g. V4L2_PIX_FMT_VP9) if those are supported by Chrome. It returns
// V4L2_PIX_FMT_INVALID otherwise.
uint32_t VideoCodecProfileToV4L2PixFmt(VideoCodecProfile profile,
                                       bool slice_based);

// Translates a POSIX |timeval| to Chrome's base::TimeDelta.
base::TimeDelta TimeValToTimeDelta(const struct timeval& timeval);

// Translates a Chrome |time_delta| to a POSIX struct timeval.
struct timeval TimeDeltaToTimeVal(base::TimeDelta time_delta);

// Return a set of all the codecs supported by the hardware as well as
// their capabilities
std::optional<SupportedVideoDecoderConfigs> GetSupportedV4L2DecoderConfigs();

// Queries the driver to see if it supports stateful decoding.
bool IsV4L2DecoderStateful();

// Queries whether V4L2 virtual driver (VISL) is used on VM.
bool IsVislDriver();

// Returns a readable description of |ctrls|.
std::string V4L2ControlsToString(const struct v4l2_ext_controls* ctrls);

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_UTILS_H_
