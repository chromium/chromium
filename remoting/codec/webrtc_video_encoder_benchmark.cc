// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

#define BENCHMARK_LOG() LAZY_STREAM(LOG_STREAM(INFO), LOG_IS_ON(INFO))

enum class Codec {
  kAv1,
  kVp8,
  kVp9,
};

enum class Profile {
  k0,
  k1,
};

enum class ColorSpace {
  kI420,
  kI444,
};

struct EncoderParams {
  Codec codec = Codec::kAv1;
  Profile profile = Profile::k0;
  ColorSpace color_space = ColorSpace::kI420;
  bool use_active_map = false;
  int bitrate_kbps = 5000;
};

struct RuntimeParams {
  int duration_s = 10;
  double fps = 30.0;
  int max_frames = 100;
  base::FilePath frame_dir;
  base::FilePath chrome_path;

  // Fields that change per iteration (scenario/size)
  int frame_count = 0;
  webrtc::DesktopSize size;
  std::string scenario;
};

std::string CodecToString(Codec codec) {
  switch (codec) {
    case Codec::kAv1:
      return "av1";
    case Codec::kVp8:
      return "vp8";
    case Codec::kVp9:
      return "vp9";
  }
}

class EncoderBenchmark {
 public:
  EncoderBenchmark() = default;
  ~EncoderBenchmark() = default;

  void Run(const EncoderParams& encoder_params,
           const RuntimeParams& runtime_params) {
    BENCHMARK_LOG() << "EncoderBenchmark::Run not implemented yet.";
    BENCHMARK_LOG() << "Codec: " << CodecToString(encoder_params.codec);
    BENCHMARK_LOG() << "Profile: "
                    << (encoder_params.profile == Profile::k1 ? "1" : "0");
    BENCHMARK_LOG() << "Color Space: "
                    << (encoder_params.color_space == ColorSpace::kI444 ? "I444"
                                                                       : "I420");
    BENCHMARK_LOG() << "Use Active Map: "
                    << (encoder_params.use_active_map ? "enabled" : "disabled");
    BENCHMARK_LOG() << "Frame count: " << runtime_params.frame_count;
    BENCHMARK_LOG() << "FPS: " << runtime_params.fps;
    BENCHMARK_LOG() << "Bitrate: " << encoder_params.bitrate_kbps << " kbps";
    BENCHMARK_LOG() << "Size: " << runtime_params.size.width() << "x"
                    << runtime_params.size.height();
    BENCHMARK_LOG() << "Scenario: " << runtime_params.scenario;
  }
};

void PrintHelp(const char* executable_name) {
  std::cout << "Usage: " << executable_name << R"([options]
Options:
  --codec=<av1|vp8|vp9>  Codec to benchmark (default: av1)
  --profile=<0|1>        Encoder profile (default: 0)
  --use-active-map=<B>   Override default active-map usage
  --bitrate=<N>          Target bitrate in kbps (default: 5000)
  --duration=<N>         Duration in seconds (default: 10)
  --fps=<N>              Frames per second (default: 30)
  --max-frames=<N>       Max frames to preload (default: 100)
  --scenarios=<S>        Comma-separated scenarios, or 'all'
  --sizes=<S>            Comma-separated sizes (e.g., 1080p,4k,800x600)
  --frame-dir=<P>        Path to pre-generated frames
  --chrome-path=<P>      Path to the Chrome/Chromium binary
)";
}

bool ParseCommandLine(const base::CommandLine* cmd_line,
                      EncoderParams& encoder_params,
                      RuntimeParams& runtime_params,
                      std::vector<std::string>& scenarios,
                      std::vector<webrtc::DesktopSize>& sizes) {
  std::string codec_str =
      base::ToLowerASCII(cmd_line->GetSwitchValueASCII("codec"));
  if (codec_str.empty() || codec_str == "av1") {
    encoder_params.codec = Codec::kAv1;
  } else if (codec_str == "vp8") {
    encoder_params.codec = Codec::kVp8;
  } else if (codec_str == "vp9") {
    encoder_params.codec = Codec::kVp9;
  } else {
    LOG(ERROR) << "Invalid codec: " << codec_str;
    return false;
  }

  int profile_val = 0;
  if (cmd_line->HasSwitch("profile")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("profile"),
                           &profile_val) ||
        (profile_val != 0 && profile_val != 1)) {
      LOG(ERROR) << "Invalid profile: "
                 << cmd_line->GetSwitchValueASCII("profile")
                 << ". Only 0 and 1 are supported.";
      return false;
    }
  }
  encoder_params.profile = (profile_val == 1) ? Profile::k1 : Profile::k0;
  encoder_params.color_space = (encoder_params.profile == Profile::k1)
                                   ? ColorSpace::kI444
                                   : ColorSpace::kI420;

  if (encoder_params.codec == Codec::kVp8 &&
      encoder_params.profile != Profile::k0) {
    LOG(ERROR) << "VP8 only supports profile 0.";
    return false;
  }

  encoder_params.use_active_map = (encoder_params.codec != Codec::kAv1);
  if (cmd_line->HasSwitch("use-active-map")) {
    std::string value =
        base::ToLowerASCII(cmd_line->GetSwitchValueASCII("use-active-map"));
    if (value == "true" || value == "1") {
      encoder_params.use_active_map = true;
    } else if (value == "false" || value == "0") {
      encoder_params.use_active_map = false;
    } else {
      LOG(ERROR) << "Invalid value for --use-active-map: " << value;
      return false;
    }
  }

  encoder_params.bitrate_kbps = 5000;
  if (cmd_line->HasSwitch("bitrate")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("bitrate"),
                           &encoder_params.bitrate_kbps)) {
      LOG(ERROR) << "Invalid bitrate: "
                 << cmd_line->GetSwitchValueASCII("bitrate");
      return false;
    }
  }

  runtime_params.duration_s = 10;
  if (cmd_line->HasSwitch("duration")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("duration"),
                           &runtime_params.duration_s)) {
      LOG(ERROR) << "Invalid duration: "
                 << cmd_line->GetSwitchValueASCII("duration");
      return false;
    }
  }

  runtime_params.fps = 30.0;
  if (cmd_line->HasSwitch("fps")) {
    if (!base::StringToDouble(cmd_line->GetSwitchValueASCII("fps"),
                              &runtime_params.fps)) {
      LOG(ERROR) << "Invalid fps: " << cmd_line->GetSwitchValueASCII("fps");
      return false;
    }
  }

  if (runtime_params.fps <= 0) {
    LOG(ERROR) << "FPS must be greater than zero.";
    return false;
  }

  runtime_params.max_frames = 100;
  if (cmd_line->HasSwitch("max-frames")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("max-frames"),
                           &runtime_params.max_frames)) {
      LOG(ERROR) << "Invalid max-frames: "
                 << cmd_line->GetSwitchValueASCII("max-frames");
      return false;
    }
  }

  runtime_params.frame_dir = cmd_line->GetSwitchValuePath("frame-dir");
  runtime_params.chrome_path = cmd_line->GetSwitchValuePath("chrome-path");

  if (!runtime_params.frame_dir.empty()) {
    scenarios = {"user_provided_frames"};
  } else {
    std::string scenarios_str = cmd_line->GetSwitchValueASCII("scenarios");
    if (scenarios_str == "all" || scenarios_str.empty()) {
      scenarios = {"desktop_clock", "scroll_vertical", "scroll_horizontal",
                   "moving_window", "busy_developer",  "low_motion",
                   "high_motion",   "chaos_stress"};
    } else {
      scenarios = base::SplitString(scenarios_str, ",", base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY);
    }
  }

  if (cmd_line->HasSwitch("sizes")) {
    std::string sizes_str = cmd_line->GetSwitchValueASCII("sizes");
    std::vector<std::string> size_strs = base::SplitString(
        sizes_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& size_str : size_strs) {
      if (size_str == "1080p") {
        sizes.emplace_back(1920, 1080);
      } else if (size_str == "1440p") {
        sizes.emplace_back(2560, 1440);
      } else if (size_str == "4k" || size_str == "2160p") {
        sizes.emplace_back(3840, 2160);
      } else {
        std::vector<std::string> dims = base::SplitString(
            size_str, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        if (dims.size() == 2) {
          int width, height;
          if (base::StringToInt(dims[0], &width) &&
              base::StringToInt(dims[1], &height)) {
            sizes.emplace_back(width, height);
          } else {
            LOG(ERROR) << "Invalid size dimensions: " << size_str;
            return false;
          }
        } else {
          LOG(ERROR) << "Invalid size format (expected WxH or name): "
                     << size_str;
          return false;
        }
      }
    }
  } else {
    sizes = {{1920, 1080}, {2560, 1440}, {3840, 2160}};
  }

  return true;
}

}  // namespace

}  // namespace remoting

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (cmd_line->HasSwitch("help")) {
    remoting::PrintHelp(argv[0]);
    return 0;
  }

  TestTimeouts::Initialize();
  base::test::TaskEnvironment task_environment;

  remoting::EncoderParams encoder_params;
  remoting::RuntimeParams base_runtime_params;
  std::vector<std::string> scenarios;
  std::vector<webrtc::DesktopSize> sizes;

  if (!remoting::ParseCommandLine(cmd_line, encoder_params, base_runtime_params,
                                  scenarios, sizes)) {
    return 1;
  }

  base_runtime_params.frame_count = static_cast<int>(
      base_runtime_params.duration_s * base_runtime_params.fps);
  if (base_runtime_params.frame_count > base_runtime_params.max_frames) {
    LOG(WARNING) << "Capping frame count at " << base_runtime_params.max_frames
                 << " to save memory. (4K frames are ~33MB each). Use "
                    "--max-frames to override.";
    base_runtime_params.frame_count = base_runtime_params.max_frames;
  }

  remoting::EncoderBenchmark benchmark;
  for (const auto& scenario : scenarios) {
    for (const auto& size : sizes) {
      remoting::RuntimeParams runtime_params = base_runtime_params;
      runtime_params.size = size;
      runtime_params.scenario = scenario;

      benchmark.Run(encoder_params, runtime_params);
    }
  }

  return 0;
}
