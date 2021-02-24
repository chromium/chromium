// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <va/va.h>

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/gpu/vaapi/test/vp9_decoder.h"
#include "media/gpu/vaapi/va_stubs.h"
#include "ui/gfx/geometry/size.h"

using media::vaapi_test::VaapiDevice;
using media::vaapi_test::VideoDecoder;
using media::vaapi_test::Vp9Decoder;
using media_gpu_vaapi::InitializeStubs;
using media_gpu_vaapi::kModuleVa;
using media_gpu_vaapi::kModuleVa_drm;
#if BUILDFLAG(IS_CHROMEOS_ASH)
using media_gpu_vaapi::kModuleVa_prot;
#endif
using media_gpu_vaapi::StubPathMap;

#define fourcc(a, b, c, d)                                             \
  ((static_cast<uint32_t>(a) << 0) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

namespace {

constexpr char kUsageMsg[] =
    "usage: decode_test\n"
    "           --video=<video path>\n"
    "           [--frames=<number of frames to decode>]\n"
    "           [--out-prefix=<path prefix of decoded frame PNGs>]\n"
    "           [--md5]\n"
    "           [--visible]\n"
    "           [--loop]\n"
    "           [--v=<log verbosity>]\n"
    "           [--help]\n";

constexpr char kHelpMsg[] =
    "This binary decodes the IVF video in <video> path with specified video\n"
    "<profile> via thinly wrapped libva calls.\n"
    "Supported codecs: VP9 (profiles 0, 2)\n"
    "\nThe following arguments are supported:\n"
    "    --video=<path>\n"
    "        Required. Path to IVF-formatted video to decode.\n"
    "    --frames=<int>\n"
    "        Optional. Number of frames to decode, defaults to all.\n"
    "        Override with a positive integer to decode at most that many.\n"
    "    --out-prefix=<string>\n"
    "        Optional. Save PNGs of decoded (and visible, if --visible is\n"
    "        specified) frames if and only if a path prefix (which may\n"
    "        specify a directory) is provided here, resulting in\n"
    "        e.g. frame_0.png, frame_1.png, etc. if passed \"frame\".\n"
    "        If specified along with --loop (see below), only saves the first\n"
    "        iteration of decoded frames.\n"
    "        If omitted, the output of this binary is error or lack thereof.\n"
    "    --md5\n"
    "        Optional. If specified, prints the md5 of each decoded (and\n"
    "        visible, if --visible is specified) frame in I420 format to\n"
    "        stdout.\n"
    "        Only supported when vaDeriveImage() produces I420 and NV12 data\n"
    "        for all frames in the video.\n"
    "    --visible\n"
    "        Optional. If specified, applies post-decode processing (PNG\n"
    "        output, md5 hash) only to visible frames.\n"
    "    --loop\n"
    "        Optional. If specified, loops decoding until terminated\n"
    "        externally or until an error occurs, at which point the current\n"
    "        pass through the video completes and the binary exits.\n"
    "        If specified with --frames, loops decoding that number of\n"
    "        leading frames. If specified with --out-prefix, loops decoding,\n"
    "        but only saves the first iteration of decoded frames.\n"
    "    --help\n"
    "        Display this help message and exit.\n";

// Returns string representation of |fourcc|.
std::string FourccStr(uint32_t fourcc) {
  std::stringstream s;
  s << static_cast<char>(fourcc & 0xFF)
    << static_cast<char>((fourcc >> 8) & 0xFF)
    << static_cast<char>((fourcc >> 16) & 0xFF)
    << static_cast<char>((fourcc >> 24) & 0xFF);
  return s.str();
}

// Creates the appropriate decoder for |stream_data| which is expected to point
// to IVF data of length |stream_len|. The decoder will use |va_device| to issue
// VAAPI calls. Returns nullptr on failure.
std::unique_ptr<VideoDecoder> CreateDecoder(const VaapiDevice& va_device,
                                            const uint8_t* stream_data,
                                            size_t stream_len) {
  // Set up video parser.
  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};
  if (!ivf_parser->Initialize(stream_data, stream_len, &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser";
    return nullptr;
  }

  // Create appropriate decoder for codec.
  VLOG(1) << "Creating decoder with codec " << FourccStr(file_header.fourcc);
  if (file_header.fourcc == fourcc('V', 'P', '9', '0'))
    return std::make_unique<Vp9Decoder>(std::move(ivf_parser), va_device);

  LOG(ERROR) << "Codec " << FourccStr(file_header.fourcc) << " not supported.\n"
             << kUsageMsg;
  return nullptr;
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
    std::cout << "No input video path provided to decode.\n" << kUsageMsg;
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
  bool first_loop = true;
  bool errored = false;

  do {
    const std::unique_ptr<VideoDecoder> dec =
        CreateDecoder(va_device, stream.data(), stream.length());
    if (!dec) {
      LOG(ERROR) << "Failed to create decoder for file: " << video_path;
      return EXIT_FAILURE;
    }

    for (int i = 0; i < n_frames || n_frames == 0; i++) {
      LOG(INFO) << "Frame " << i << "...";
      const VideoDecoder::Result res = dec->DecodeNextFrame();

      if (res == VideoDecoder::kEOStream) {
        LOG(INFO) << "End of stream.";
        break;
      }

      if (res == VideoDecoder::kFailed) {
        LOG(ERROR) << "Failed to decode.";
        errored = true;
        continue;
      }

      if (cmd->HasSwitch("visible") && !dec->LastDecodedFrameVisible())
        continue;

      if (!output_prefix.empty() && first_loop) {
        dec->LastDecodedFrameToPNG(
            base::StringPrintf("%s_%d.png", output_prefix.c_str(), i));
      }
      if (cmd->HasSwitch("md5")) {
        std::cout << dec->LastDecodedFrameMD5Sum() << std::endl;
      }
    }

    first_loop = false;
  } while (loop_decode && !errored);

  LOG(INFO) << "Done reading.";

  if (errored)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
