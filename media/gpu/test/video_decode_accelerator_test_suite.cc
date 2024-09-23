// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_decode_accelerator_test_suite.h"
#include "base/test/task_environment.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_bitstream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video decoder tests usage help message. Make sure to also update
// the documentation under docs/media/gpu/video_decoder_test_usage.md
// when making changes here.
constexpr const char* usage_msg =
    R"(usage: video_decode_accelerator_tests
           [-v=<level>] [--vmodule=<config>]
           [--validator_type=(none|md5|ssim)]
           [--output_frames=(all|corrupt)] [--output_format=(png|yuv)]
           [--output_limit=<number>] [--output_folder=<folder>]
           [--linear_output] ([--use-legacy]|[--use_vd_vda])
           [--use-gl=<backend>] [--gtest_help] [--help]
           [<video path>] [<video metadata path>]
)";

// Video decoder tests help message.
const std::string help_msg =
    std::string(
        R"""(Run the video decode accelerator tests on the video specified by
<video path>. If no <video path> is given the default
"test-25fps.h264" video will be used.

The <video metadata path> should specify the location of a json file
containing the video's metadata, such as frame checksums. By default
<video path>.json will be used.

The following arguments are supported:
   -v                   enable verbose mode, e.g. -v=2.
  --vmodule             enable verbose mode for the specified module,
                        e.g. --vmodule=*media/gpu*=2.

  --validator_type      validate decoded frames, possible values are
                        md5 (default, compare against md5hash of expected
                        frames), ssim (compute SSIM against expected
                        frames, currently allowed for AV1 streams only)
                        and none (disable frame validation).
  --use-legacy          use the legacy VDA-based video decoders.
  --use_vd_vda          use the new VD-based video decoders with a
                        wrapper that translates to the VDA interface,
                        used to test interaction with older components
  --linear_output       use linear buffers as the final output of the
                        decoder which may require the use of an image
                        processor internally. This flag only works in
                        conjunction with --use_vd_vda.
                        Disabled by default.
  --output_frames       write the selected video frames to disk, possible
                        values are "all|corrupt".
  --output_format       set the format of frames saved to disk, supported
                        formats are "png" (default) and "yuv".
  --output_limit        limit the number of frames saved to disk.
  --output_folder       set the folder used to store frames, defaults to
                        "<testname>".
  --use-gl              specify which GPU backend to use, possible values
                        include desktop (GLX), egl (GLES w/ ANGLE), and
                        swiftshader (software rendering)""") +
#if defined(ARCH_CPU_ARM_FAMILY)
    R"""(
  --disable-libyuv      use hw format conversion instead of libYUV.
                        libYUV will be used by default, unless the
                        video decoder format is not supported;
                        in that case the code will try to use the
                        v4l2 image processor.)""" +
#endif  // defined(ARCH_CPU_ARM_FAMILY)
    R"""(
  --gtest_help          display the gtest help and exit.
  --help                display this help and exit.
)""";

}  // namespace

std::unique_ptr<base::test::TaskEnvironment>
    VideoDecodeAcceleratorTestSuite::task_environment_;

// static
VideoDecodeAcceleratorTestSuite* VideoDecodeAcceleratorTestSuite::Create(
    int argc,
    char** argv) {
  // Set the default test data path.
  media::test::VideoBitstream::SetTestDataPath(media::GetTestDataPath());

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::test::usage_msg << "\n" << media::test::help_msg;
    return nullptr;
  }

  // Check if a video was specified on the command line.
  base::CommandLine::StringVector args = cmd_line->GetArgs();

  base::FilePath video_path =
      (args.size() >= 1) ? base::FilePath(args[0]) : base::FilePath();
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();

  // Parse command line arguments.
  auto validator_type =
      media::test::VideoPlayerTestEnvironment::ValidatorType::kMD5;
  media::test::FrameOutputConfig frame_output_config;
  base::FilePath::StringType output_folder = base::FilePath::kCurrentDirectory;
  bool use_legacy = false;
  bool use_vd_vda = false;
  bool linear_output = false;
  std::vector<base::test::FeatureRef> disabled_features;
  std::vector<base::test::FeatureRef> enabled_features;

  media::test::DecoderImplementation implementation =
      media::test::DecoderImplementation::kVD;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();

  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||  // Handled by GoogleTest
                                          // Options below are handled by Chrome
        it->first == "use-gl" || it->first == "v" || it->first == "vmodule" ||
        it->first == "enable-features" || it->first == "disable-features" ||
        it->first == "test-launcher-shard-index" ||
        it->first == "test-launcher-summary-output" ||
        it->first == "test-launcher-total-shards" ||
        it->first == "enable-primary-node-access-for-vkms-testing" ||
        it->first == "single-process-tests" ||
        it->first == "test-launcher-output" ||
        it->first == "test-launcher-retries-left" ||
        it->first == "enable-clear-hevc-for-testing") {
      continue;
    }

    if (it->first == "validator_type") {
      auto validator_type_str = cmd_line->GetSwitchValueASCII("validator_type");
      if (validator_type_str == "none") {
        validator_type =
            media::test::VideoPlayerTestEnvironment::ValidatorType::kNone;
      } else if (validator_type_str == "md5") {
        validator_type =
            media::test::VideoPlayerTestEnvironment::ValidatorType::kMD5;
      } else if (validator_type_str == "ssim") {
        validator_type =
            media::test::VideoPlayerTestEnvironment::ValidatorType::kSSIM;
      } else {
        std::cout << "unknown validator type \"" << validator_type_str
                  << "\", possible values are \"none|md5|ssim\"\n";
        return nullptr;
      }
    } else if (it->first == "output_frames") {
      auto output_frames_str = cmd_line->GetSwitchValueASCII("output_frames");
      if (output_frames_str == "all") {
        frame_output_config.output_mode = media::test::FrameOutputMode::kAll;
      } else if (output_frames_str == "corrupt") {
        frame_output_config.output_mode =
            media::test::FrameOutputMode::kCorrupt;
      } else {
        std::cout << "unknown frame output mode \"" << output_frames_str
                  << "\", possible values are \"all|corrupt\"\n";
        return nullptr;
      }
    } else if (it->first == "output_format") {
      auto output_format_str = cmd_line->GetSwitchValueASCII("output_format");
      if (output_format_str == "png") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kPNG;
      } else if (output_format_str == "yuv") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kYUV;
      } else {
        std::cout << "unknown frame output format \"" << output_format_str
                  << "\", possible values are \"png|yuv\"\n";
        return nullptr;
      }
    } else if (it->first == "output_limit") {
      if (!base::StringToUint64(it->second,
                                &frame_output_config.output_limit)) {
        std::cout << "invalid number \"" << it->second << "\n";
        return nullptr;
      }
    } else if (it->first == "output_folder") {
      output_folder = it->second;
    } else if (it->first == "video") {
      video_path = base::FilePath(it->second);
    } else if (it->first == "use-legacy") {
      use_legacy = true;
      implementation = media::test::DecoderImplementation::kVDA;
    } else if (it->first == "use_vd_vda") {
      use_vd_vda = true;
      implementation = media::test::DecoderImplementation::kVDVDA;
    } else if (it->first == "linear_output") {
      linear_output = true;
#if defined(ARCH_CPU_ARM_FAMILY)
    } else if (it->first == "disable-libyuv") {
      // TODO(bchoobineh): implement disabling libyuv here.
#endif  // defined(ARCH_CPU_ARM_FAMILY)
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return nullptr;
    }
  }
  disabled_features.push_back(media::kGlobalVaapiLock);

  if (use_legacy && use_vd_vda) {
    std::cout << "--use-legacy and --use_vd_vda cannot be enabled together.\n"
              << media::test::usage_msg;
    return nullptr;
  }
  if (linear_output && !use_vd_vda) {
    std::cout << "--linear_output must be used with the VDVDA (--use_vd_vda)\n"
                 "implementation.\n"
              << media::test::usage_msg;
    return nullptr;
  }

  // Add the command line flag for HEVC testing which will be checked by the
  // video decoder to allow clear HEVC decoding.
  cmd_line->AppendSwitch("enable-clear-hevc-for-testing");
  cmd_line->AppendSwitchPath("video", video_path);

#if defined(ARCH_CPU_ARM_FAMILY) && BUILDFLAG(IS_CHROMEOS)
  // On some platforms bandwidth compression is fully opaque and can not be read
  // by the cpu. This prevents MD5 computation as that is done by the CPU. This
  // is currently only needed for Trogdor/Strongbad to disable UBWC compression.
  setenv("MINIGBM_DEBUG", "nocompression", 1);
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
  // For V4L2 testing with VISL, dumb driver is used with vkms for minigbm
  // backend. In this case, the primary node needs to be used instead of the
  // render node.
  cmd_line->AppendSwitch(switches::kEnablePrimaryNodeAccessForVkmsTesting);

  std::unique_ptr<base::FeatureList> feature_list =
      std::make_unique<base::FeatureList>();
  feature_list->InitFromCommandLine(
      cmd_line->GetSwitchValueASCII(switches::kEnableFeatures),
      cmd_line->GetSwitchValueASCII(switches::kDisableFeatures));
  if (feature_list->IsFeatureOverridden("V4L2FlatStatefulVideoDecoder")) {
    enabled_features.push_back(media::kV4L2FlatStatefulVideoDecoder);
  }
  if (feature_list->IsFeatureOverridden("V4L2FlatVideoDecoder")) {
    enabled_features.push_back(media::kV4L2FlatStatefulVideoDecoder);
  }
#endif

  return new VideoDecodeAcceleratorTestSuite(
      argc, argv, video_path, video_metadata_path, validator_type,
      implementation, linear_output, base::FilePath(output_folder),
      frame_output_config, enabled_features, disabled_features);
}

VideoDecodeAcceleratorTestSuite::VideoDecodeAcceleratorTestSuite(
    int argc,
    char** argv,
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    VideoPlayerTestEnvironment::ValidatorType validator_type,
    const DecoderImplementation implementation,
    bool linear_output,
    const base::FilePath& output_folder,
    const FrameOutputConfig& frame_output_config,
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features)
    : base::TestSuite(argc, argv),
      video_test_env_(std::move(media::test::VideoPlayerTestEnvironment::Create(
          video_path,
          video_metadata_path,
          validator_type,
          implementation,
          linear_output,
          base::FilePath(output_folder),
          frame_output_config,
          enabled_features,
          disabled_features,
          false))) {}

VideoDecodeAcceleratorTestSuite::~VideoDecodeAcceleratorTestSuite() = default;

const media::test::VideoBitstream* VideoDecodeAcceleratorTestSuite::Video()
    const {
  return video_test_env_->Video();
}

bool VideoDecodeAcceleratorTestSuite::IsValidatorEnabled() const {
  return video_test_env_->IsValidatorEnabled();
}

VideoPlayerTestEnvironment::ValidatorType
VideoDecodeAcceleratorTestSuite::GetValidatorType() const {
  return video_test_env_->GetValidatorType();
}

DecoderImplementation
VideoDecodeAcceleratorTestSuite::GetDecoderImplementation() const {
  return video_test_env_->GetDecoderImplementation();
}

bool VideoDecodeAcceleratorTestSuite::ShouldOutputLinearBuffers() const {
  return video_test_env_->ShouldOutputLinearBuffers();
}

FrameOutputMode VideoDecodeAcceleratorTestSuite::GetFrameOutputMode() const {
  return video_test_env_->GetFrameOutputMode();
}

VideoFrameFileWriter::OutputFormat
VideoDecodeAcceleratorTestSuite::GetFrameOutputFormat() const {
  return video_test_env_->GetFrameOutputFormat();
}

uint64_t VideoDecodeAcceleratorTestSuite::GetFrameOutputLimit() const {
  return video_test_env_->GetFrameOutputLimit();
}

const base::FilePath& VideoDecodeAcceleratorTestSuite::OutputFolder() const {
  return video_test_env_->OutputFolder();
}

base::FilePath VideoDecodeAcceleratorTestSuite::GetTestOutputFilePath() const {
  return video_test_env_->GetTestOutputFilePath();
}

bool VideoDecodeAcceleratorTestSuite::ValidVideoTestEnv() const {
  return !!video_test_env_;
}

bool VideoDecodeAcceleratorTestSuite::IsV4L2VirtualDriver() const {
  return video_test_env_->IsV4L2VirtualDriver();
}

void VideoDecodeAcceleratorTestSuite::Initialize() {
  base::TestSuite::Initialize();

  CHECK(!task_environment_);
  task_environment_ = std::make_unique<base::test::TaskEnvironment>();
}

void VideoDecodeAcceleratorTestSuite::Shutdown() {
  CHECK(task_environment_);
  task_environment_ = nullptr;

  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace media
