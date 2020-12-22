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
    "        Optional. Save PNGs of decoded frames if and only if a path\n"
    "        prefix (which may specify a directory) is provided here,\n"
    "        resulting in e.g. frame_0.png, frame_1.png, etc. if passed\n"
    "        \"frame\".\n"
    "        If omitted, the output of this binary is error or lack thereof.\n"
    "    --help\n"
    "        Display this help message and exit.\n";

// Creates the appropriate decoder for the given |fourcc|.
std::unique_ptr<VideoDecoder> CreateDecoder(
    uint32_t fourcc,
    std::unique_ptr<media::IvfParser> ivf_parser,
    const VaapiDevice& va_device) {
  if (fourcc == fourcc('V', 'P', '9', '0'))
    return std::make_unique<Vp9Decoder>(std::move(ivf_parser), va_device);

  return nullptr;
}

// Returns string representation of |fourcc|.
std::string FourccStr(uint32_t fourcc) {
  std::stringstream s;
  s << static_cast<char>(fourcc & 0xFF)
    << static_cast<char>((fourcc >> 8) & 0xFF)
    << static_cast<char>((fourcc >> 16) & 0xFF)
    << static_cast<char>((fourcc >> 24) & 0xFF);
  return s.str();
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

  // Set up video stream and parser.
  base::MemoryMappedFile stream;
  if (!stream.Initialize(video_path)) {
    LOG(ERROR) << "Couldn't open file: " << video_path;
    return EXIT_FAILURE;
  }

  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};
  if (!ivf_parser->Initialize(stream.data(), stream.length(), &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser for file: " << video_path;
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

  const VaapiDevice va_device;
  std::unique_ptr<VideoDecoder> dec =
      CreateDecoder(file_header.fourcc, std::move(ivf_parser), va_device);
  if (!dec) {
    LOG(ERROR) << "Codec " << FourccStr(file_header.fourcc)
               << " not supported.\n"
               << kUsageMsg;
    return EXIT_FAILURE;
  }
  VLOG(1) << "Created decoder for codec " << FourccStr(file_header.fourcc);

  VideoDecoder::Result res;
  int i = 0;
  bool errored = false;
  while (true) {
    LOG(INFO) << "Frame " << i << "...";
    res = dec->DecodeNextFrame();

    if (res == VideoDecoder::kEOStream) {
      LOG(INFO) << "End of stream.";
      break;
    }

    if (res == VideoDecoder::kFailed) {
      LOG(ERROR) << "Failed to decode.";
      errored = true;
      continue;
    }

    if (!output_prefix.empty()) {
      dec->LastDecodedFrameToPNG(
          base::StringPrintf("%s_%d.png", output_prefix.c_str(), i));
    }

    if (++i == n_frames)
      break;
  };

  LOG(INFO) << "Done reading.";

  if (errored)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
