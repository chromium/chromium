// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/videodev2.h>

#include <iostream>
#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/video_types.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/v4l2/test/vp9_decoder.h"

using media::v4l2_test::Vp9Decoder;

namespace {

constexpr char kUsageMsg[] =
    "usage: v4l2_stateless_decoder\n"
    "           --video=<video path>\n"
    "           [--frames=<number of frames to decode>]\n"
    "           [--v=<log verbosity>]\n"
    "           [--help]\n";

constexpr char kHelpMsg[] =
    "This binary decodes the IVF video in <video> path with specified \n"
    "video <profile> via thinly wrapped v4l2 calls.\n"
    "Supported codecs: VP9 (profile 0)\n"
    "\nThe following arguments are supported:\n"
    "    --video=<path>\n"
    "        Required. Path to IVF-formatted video to decode.\n"
    "    --frames=<int>\n"
    "        Optional. Number of frames to decode, defaults to all.\n"
    "        Override with a positive integer to decode at most that many.\n"
    "    --help\n"
    "        Display this help message and exit.\n";

}  // namespace

// For stateless API, fourcc |VP9F| is needed instead of |VP90| for VP9 codec.
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/pixfmt-compressed.html
// Converts fourcc |VP90| from file header to fourcc |VP9F|, which is
// a format supported on driver.
uint32_t FileFourccToDriverFourcc(uint32_t header_fourcc) {
  if (header_fourcc == V4L2_PIX_FMT_VP9) {
    LOG(INFO) << "OUTPUT format mapped from VP90 to VP9F.";
    return V4L2_PIX_FMT_VP9_FRAME;
  }

  return header_fourcc;
}

// Creates the appropriate decoder for |stream|, which points to IVF data.
// Returns nullptr on failure.
std::unique_ptr<Vp9Decoder> CreateDecoder(
    const base::MemoryMappedFile& stream) {
  CHECK(stream.IsValid());

  // Set up video parser.
  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};

  if (!ivf_parser->Initialize(stream.data(), stream.length(), &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser";
    return nullptr;
  }

  // Create appropriate decoder for codec.
  VLOG(1) << "Creating decoder with codec "
          << media::FourccToString(file_header.fourcc);

  LOG_ASSERT(file_header.fourcc == v4l2_fourcc('V', 'P', '9', '0'))
      << "Codec " << media::FourccToString(file_header.fourcc)
      << " not supported.\n"
      << kUsageMsg;

  const auto driver_codec_fourcc = FileFourccToDriverFourcc(file_header.fourcc);

  CHECK_EQ(driver_codec_fourcc, V4L2_PIX_FMT_VP9_FRAME)
      << "Only VP9 is supported, got: "
      << media::FourccToString(driver_codec_fourcc);

  return Vp9Decoder::Create(std::move(ivf_parser), file_header);
}

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
  if (video_path.empty())
    LOG(FATAL) << "No input video path provided to decode.\n" << kUsageMsg;

  const std::string frames = cmd->GetSwitchValueASCII("frames");
  int n_frames;
  if (frames.empty()) {
    n_frames = 0;
  } else if (!base::StringToInt(frames, &n_frames) || n_frames <= 0) {
    LOG(FATAL) << "Number of frames to decode must be positive integer, got "
               << frames;
  }

  // Set up video stream.
  base::MemoryMappedFile stream;
  if (!stream.Initialize(video_path))
    LOG(FATAL) << "Couldn't open file: " << video_path;

  const std::unique_ptr<Vp9Decoder> dec = CreateDecoder(stream);
  if (!dec)
    LOG(FATAL) << "Failed to create decoder for file: " << video_path;

  if (!dec->Initialize())
    LOG(FATAL) << "Initialization for decoding failed.";

  return EXIT_SUCCESS;
}
