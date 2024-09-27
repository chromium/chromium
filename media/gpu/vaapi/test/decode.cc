// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <va/va.h>

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/vaapi/test/av1_decoder.h"
#include "media/gpu/vaapi/test/h264_decoder.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/gpu/vaapi/test/vp8_decoder.h"
#include "media/gpu/vaapi/test/vp9_decoder.h"
#include "media/gpu/vaapi/va_stubs.h"
#include "media/media_buildflags.h"
#include "media/parsers/ivf_parser.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/gpu/vaapi/test/h265_decoder.h"
#endif

using media::vaapi_test::Av1Decoder;
using media::vaapi_test::H264Decoder;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
using media::vaapi_test::H265Decoder;
#endif
using media::vaapi_test::SharedVASurface;
using media::vaapi_test::VaapiDevice;
using media::vaapi_test::VideoDecoder;
using media::vaapi_test::Vp8Decoder;
using media::vaapi_test::Vp9Decoder;
using media_gpu_vaapi::InitializeStubs;
using media_gpu_vaapi::kModuleVa;
using media_gpu_vaapi::kModuleVa_drm;
#if BUILDFLAG(IS_CHROMEOS_ASH)
using media_gpu_vaapi::kModuleVa_prot;
#endif
using media_gpu_vaapi::StubPathMap;

namespace {

constexpr char kUsageMsg[] =
    "usage: decode_test\n"
    "           --video=<video path>\n"
    "           --codec=<codec name>\n"
    "           [--frames=<number of frames to decode>]\n"
    "           [--fetch=<derive|get>]\n"
    "           [--out-prefix=<path prefix of decoded frame PNGs>]\n"
    "           [--md5[=<checksum path>]]\n"
    "           [--visible]\n"
    "           [--loop[=<n>]]\n"
    "           [--progress]\n"
    "           [--v=<log verbosity>]\n"
    "           [--help]\n";

constexpr char kHelpMsg[] =
    "This binary decodes the IVF video in <video> path with specified video\n"
    "<profile> via thinly wrapped libva calls.\n"
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    "Supported codecs: VP8, VP9 (profiles 0, 2), AV1 (profile 0), H264, and\n"
    "H265.\n"
#else
    "Supported codecs: VP8, VP9 (profiles 0, 2), AV1 (profile 0), H264.\n"
#endif
    "\nThe following arguments are supported:\n"
    "    --video=<path>\n"
    "        Required. Path to IVF-formatted or HEVC-formatted video.\n"
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    "    --codec=<codec name>\n"
    "        Required. The case-insensitive name of the codec to be used for\n"
    "        decoding. Valid codec names are VP8, VP9, AV1, H264, and H265.\n"
#else
    "    --codec=<codec name>\n"
    "        Required. The case-insensitive name of the codec to be used for\n"
    "        decoding. Valid codec names are VP8, VP9, AV1, and H264.\n"
#endif
    "    --frames=<int>\n"
    "        Optional. Number of frames to decode, defaults to all.\n"
    "        Override with a positive integer to decode at most that many.\n"
    "    --fetch=<derive|get>\n"
    "        Optional. If omitted, try to fetch VASurface data by any means.\n"
    "        Specifically, try, in order, vaDeriveImage, then if that fails,\n"
    "        vaCreateImage + vaGetImage. Otherwise, only attempt the\n"
    "        specified fetch policy.\n"
    "        NB: (b/201587517) AMD ignores this flag and always uses\n"
    "        vaGetImage.\n"
    "    --out-prefix=<string>\n"
    "        Optional. Save PNGs of decoded (and visible, if --visible is\n"
    "        specified) frames if and only if a path prefix (which may\n"
    "        specify a directory) is provided here, resulting in\n"
    "        e.g. frame_0.png, frame_1.png, etc. if passed \"frame\".\n"
    "        If specified along with --loop (see below), only saves the first\n"
    "        iteration of decoded frames.\n"
    "        If omitted, the output of this binary is error or lack thereof.\n"
    "    --md5[=<checksum path>]\n"
    "        Optional. If specified without a value, prints the md5 of each\n"
    "        decoded (and visible, if --visible is specified) frame in I420\n"
    "        format to stdout. If specified with a value, prints the md5 to\n"
    "        the specified value instead of stdout. Only supported when\n"
    "        vaDeriveImage() produces I420 and NV12 data for all frames\n"
    "        in the video.\n"
    "    --visible\n"
    "        Optional. If specified, applies post-decode processing (PNG\n"
    "        output, md5 hash) only to visible frames.\n"
    "    --loop[=<n>]\n"
    "        Optional. If specified with a value, loops decoding specified\n"
    "        times. If specified without, loops until terminated externally\n"
    "        or until an error occurs, at which point execution will\n"
    "        immediately terminate.\n"
    "        If specified with --frames, loops decoding that number of\n"
    "        leading frames. If specified with --out-prefix, loops decoding,\n"
    "        but only saves the first iteration of decoded frames.\n"
    "    --progress\n"
    "        Optional. If specified, prints each frame number before it is\n"
    "        decoded.\n"
    "    --help\n"
    "        Display this help message and exit.\n";

// Creates the decoder for |stream_data| based on the user-provided
// value. If the user requests a valid codec that is not suitable
// for decoding |stream_data| the behavior will be undefined. Returns
// nullptr on failure.
std::unique_ptr<VideoDecoder> CreateDecoder(
    const std::string& codec,
    const VaapiDevice& va_device,
    SharedVASurface::FetchPolicy fetch_policy,
    const uint8_t* stream_data,
    size_t stream_len) {
  if (codec == "H264")
    return std::make_unique<H264Decoder>(stream_data, stream_len, va_device,
                                         fetch_policy);
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  if (codec == "H265")
    return std::make_unique<H265Decoder>(stream_data, stream_len, va_device,
                                         fetch_policy);
#endif

  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};
  if (!ivf_parser->Initialize(stream_data, stream_len, &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser";
    return nullptr;
  }

  if (codec == "AV1")
    return std::make_unique<Av1Decoder>(std::move(ivf_parser), va_device,
                                        fetch_policy);
  if (codec == "VP8")
    return std::make_unique<Vp8Decoder>(std::move(ivf_parser), va_device,
                                        fetch_policy);
  if (codec == "VP9")
    return std::make_unique<Vp9Decoder>(std::move(ivf_parser), va_device,
                                        fetch_policy);

  LOG(ERROR) << "Invalid codec requested: " << codec;
  return nullptr;
}

std::optional<SharedVASurface::FetchPolicy> GetFetchPolicy(
    const VaapiDevice& va_device,
    const std::string& fetch_policy) {
  // Always use kGetImage for AMD devices.
  // TODO(b/201587517): remove this exception.
  const std::string va_vendor_string = vaQueryVendorString(va_device.display());
  if (base::StartsWith(va_vendor_string, "Mesa Gallium driver",
                       base::CompareCase::SENSITIVE)) {
    LOG(WARNING) << "AMD driver detected, forcing vaGetImage";
    return SharedVASurface::FetchPolicy::kGetImage;
  }

  if (fetch_policy.empty())
    return SharedVASurface::FetchPolicy::kAny;
  if (base::EqualsCaseInsensitiveASCII(fetch_policy, "derive"))
    return SharedVASurface::FetchPolicy::kDeriveImage;
  if (base::EqualsCaseInsensitiveASCII(fetch_policy, "get"))
    return SharedVASurface::FetchPolicy::kGetImage;

  LOG(ERROR) << "Unrecognized fetch policy " << fetch_policy;
  return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  if (cmd->HasSwitch("help")) {
    std::cout << kUsageMsg << "\n" << kHelpMsg;
    return EXIT_SUCCESS;
  }

  const base::FilePath video_path = cmd->GetSwitchValuePath("video");
  if (video_path.empty()) {
    LOG(ERROR) << "No input video path provided to decode.";
    std::cerr << kUsageMsg;
    return EXIT_FAILURE;
  }

  const std::string codec =
      base::ToUpperASCII(cmd->GetSwitchValueASCII("codec"));
  if (codec.empty()) {
    LOG(ERROR) << "No codec string was provided.";
    std::cerr << kUsageMsg;
    return EXIT_FAILURE;
  }

  std::string output_prefix = cmd->GetSwitchValueASCII("out-prefix");

  const std::string frames = cmd->GetSwitchValueASCII("frames");
  int n_frames;
  if (frames.empty()) {
    n_frames = 0;
  } else if (!base::StringToInt(frames, &n_frames) || n_frames <= 0) {
    LOG(ERROR) << "Number of frames to decode must be positive integer, got "
               << frames;
    return EXIT_FAILURE;
  }

  std::ofstream md5_checksum_log;
  const std::string md5_checksum_log_path = cmd->GetSwitchValueASCII("md5");
  if (!md5_checksum_log_path.empty()) {
    md5_checksum_log.open(md5_checksum_log_path);
    if (!md5_checksum_log.is_open()) {
      LOG(ERROR) << "Could not open " << md5_checksum_log_path << " for writing";
      return EXIT_FAILURE;
    }
  }

  // Initialize VA stubs.
  StubPathMap paths;
  const std::string va_suffix(base::NumberToString(VA_MAJOR_VERSION + 1));
  paths[kModuleVa].push_back(std::string("libva.so.") + va_suffix);
  paths[kModuleVa_drm].push_back(std::string("libva-drm.so.") + va_suffix);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  paths[kModuleVa_prot].push_back(std::string("libva.so.") + va_suffix);
#endif
  if (!InitializeStubs(paths)) {
    LOG(ERROR) << "Failed to initialize VA stubs";
    return EXIT_FAILURE;
  }

  // Set up video stream.
  base::MemoryMappedFile stream;
  if (!stream.Initialize(video_path)) {
    LOG(ERROR) << "Couldn't open file: " << video_path;
    return EXIT_FAILURE;
  }

  const VaapiDevice va_device;
  const bool loop_decode = cmd->HasSwitch("loop");
  const std::string loops = cmd->GetSwitchValueASCII("loop");
  int n_loops;
  if (loops.empty()) {
    n_loops = 0;
  } else if (!base::StringToInt(loops, &n_loops) || n_loops <= 0) {
    LOG(ERROR) << "Number of times to loop decode must be positive integer, "
               << "got " << frames;
    return EXIT_FAILURE;
  }
  bool first_loop = true;

  const auto fetch_policy =
      GetFetchPolicy(va_device, cmd->GetSwitchValueASCII("fetch"));
  if (!fetch_policy) {
    return EXIT_FAILURE;
  }

  const bool show_progress = cmd->HasSwitch("progress");

  do {
    const std::unique_ptr<VideoDecoder> dec = CreateDecoder(
        codec, va_device, *fetch_policy, stream.data(), stream.length());
    if (!dec) {
      LOG(ERROR) << "Failed to create decoder for file: " << video_path;
      return EXIT_FAILURE;
    }

    for (int i = 0; i < n_frames || n_frames == 0; i++) {
      LOG_IF(INFO, show_progress) << "Decoding frame " << i;

      const VideoDecoder::Result res = dec->DecodeNextFrame();

      if (res == VideoDecoder::kEOStream) {
        LOG(INFO) << "End of stream.";
        break;
      }

      if (cmd->HasSwitch("visible") && !dec->LastDecodedFrameVisible())
        continue;

      if (!output_prefix.empty() && first_loop) {
        dec->LastDecodedFrameToPNG(
            base::StringPrintf("%s_%d.png", output_prefix.c_str(), i));
      }
      if (cmd->HasSwitch("md5")) {
        if (md5_checksum_log.is_open())
          md5_checksum_log << dec->LastDecodedFrameMD5Sum() << std::endl;
        else
          std::cout << dec->LastDecodedFrameMD5Sum() << std::endl;
      }
    }

    first_loop = false;
    n_loops--;
  } while (loop_decode && (loops.empty() || n_loops > 0));

  LOG(INFO) << "Done reading.";

  // Closes log file if opened.
  if (md5_checksum_log.is_open())
    md5_checksum_log.close();

  return EXIT_SUCCESS;
}
