// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/videodev2.h>

#include <iostream>
#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/video_types.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/v4l2/test/av1_decoder.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/gpu/v4l2/test/vp9_decoder.h"

using media::v4l2_test::Av1Decoder;
using media::v4l2_test::VideoDecoder;
using media::v4l2_test::Vp9Decoder;

namespace {

constexpr char kUsageMsg[] =
    "usage: v4l2_stateless_decoder\n"
    "           --video=<video path>\n"
    "           [--frames=<number of frames to decode>]\n"
    "           [--v=<log verbosity>]\n"
    "           [--output_path_prefix=<output files path prefix>]\n"
    "           [--md5]\n"
    "           [--visible]\n"
    "           [--help]\n";

constexpr char kHelpMsg[] =
    "This binary decodes the IVF video in <video> path with specified \n"
    "video <profile> via thinly wrapped v4l2 calls.\n"
    "Supported codecs: VP9 (profile 0), and AV1 (profile 0)\n"
    "\nThe following arguments are supported:\n"
    "    --video=<path>\n"
    "        Required. Path to IVF-formatted video to decode.\n"
    "    --frames=<int>\n"
    "        Optional. Number of frames to decode, defaults to all.\n"
    "        Override with a positive integer to decode at most that many.\n"
    "    --output_path_prefix=<path>\n"
    "        Optional. Prefix to the filepaths where raw YUV frames will be\n"
    "        written. For example, setting <path> to \"test/test_\" would \n"
    "        result in output files of the form \"test/test_000000.yuv\",\n"
    "       \"test/test_000001.yuv\", etc.\n"
    "    --md5\n"
    "        Optional. If specified, prints the md5 of each decoded (and\n"
    "        visible, if --visible is specified) frame in I420 format to\n"
    "        stdout.\n"
    "    --visible\n"
    "        Optional. If specified, computes md5 hash values only for\n"
    "        visible frames.\n"
    "    --help\n"
    "        Display this help message and exit.\n";

}  // namespace

// For stateless API, fourcc |VP9F| is needed instead of |VP90| for VP9 codec.
// Fourcc |AV1F| is needed instead of |AV10| for AV1 codec.
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/pixfmt-compressed.html
// Converts fourcc |VP90| or |AV01| from file header to fourcc |VP9F| or |AV1F|,
// which is a format supported on driver.
uint32_t FileFourccToDriverFourcc(uint32_t header_fourcc) {
  if (header_fourcc == V4L2_PIX_FMT_VP9) {
    LOG(INFO) << "OUTPUT format mapped from VP90 to VP9F.";
    return V4L2_PIX_FMT_VP9_FRAME;
  } else if (header_fourcc == V4L2_PIX_FMT_AV1) {
    LOG(INFO) << "OUTPUT format mapped from AV01 to AV1F.";
    return V4L2_PIX_FMT_AV1_FRAME;
  }

  return header_fourcc;
}

// Computes the md5 of given I420 data |yuv_plane| and prints the md5 to stdout.
// This functionality is needed for tast tests.
void ComputeAndPrintMd5hash(const std::vector<char>& yuv_plane) {
  base::MD5Digest md5_digest;
  base::MD5Sum(yuv_plane.data(), yuv_plane.size(), &md5_digest);
  std::cout << MD5DigestToBase16(md5_digest) << std::endl;
}

// Creates the appropriate decoder for |stream|, which points to IVF data.
// Returns nullptr on failure.
std::unique_ptr<VideoDecoder> CreateVideoDecoder(
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

  const auto driver_codec_fourcc = FileFourccToDriverFourcc(file_header.fourcc);

  if (driver_codec_fourcc == V4L2_PIX_FMT_AV1_FRAME) {
    return Av1Decoder::Create(std::move(ivf_parser), file_header);
  } else if (driver_codec_fourcc == V4L2_PIX_FMT_VP9_FRAME) {
    return Vp9Decoder::Create(std::move(ivf_parser), file_header);
  }

  LOG(ERROR) << "Codec " << media::FourccToString(file_header.fourcc)
             << " not supported.\n"
             << kUsageMsg;
  return nullptr;
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

  const bool has_output_file = cmd->HasSwitch("output_path_prefix");
  const std::string output_file_prefix =
      cmd->GetSwitchValueASCII("output_path_prefix");

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

  const std::unique_ptr<VideoDecoder> dec = CreateVideoDecoder(stream);
  if (!dec)
    LOG(FATAL) << "Failed to create decoder for file: " << video_path;

  dec->Initialize();

  for (int i = 0; i < n_frames || n_frames == 0; i++) {
    LOG(INFO) << "Frame " << i << "...";

    std::vector<char> y_plane;
    std::vector<char> u_plane;
    std::vector<char> v_plane;
    gfx::Size size;
    const VideoDecoder::Result res =
        dec->DecodeNextFrame(y_plane, u_plane, v_plane, size, i);
    if (res == VideoDecoder::kEOStream) {
      LOG(INFO) << "End of stream.";
      break;
    }

    if (cmd->HasSwitch("visible") && !dec->LastDecodedFrameVisible())
      continue;

    std::vector<char> yuv_plane(y_plane);
    yuv_plane.insert(yuv_plane.end(), u_plane.begin(), u_plane.end());
    yuv_plane.insert(yuv_plane.end(), v_plane.begin(), v_plane.end());

    if (cmd->HasSwitch("md5"))
      ComputeAndPrintMd5hash(yuv_plane);

    if (!has_output_file)
      continue;

    base::FilePath filename(
        base::StringPrintf("%s%.6d.yuv", output_file_prefix.c_str(), i));
    base::File output_file(
        filename, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    output_file.WriteAtCurrentPos(y_plane.data(), size.GetArea());
    output_file.WriteAtCurrentPos(u_plane.data(), size.GetArea() / 4);
    output_file.WriteAtCurrentPos(v_plane.data(), size.GetArea() / 4);
  }

  return EXIT_SUCCESS;
}
