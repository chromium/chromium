// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <iostream>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "remoting/codec/webrtc_video_encoder_av1.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/test/frame_generator/differ_frame_generator.h"
#include "remoting/test/frame_generator/file_frame_generator.h"
#include "remoting/test/frame_generator/headless_frame_generator.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kDefaultDuration = base::Seconds(10);
constexpr int kDefaultBitrateKbps = 5000;

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
  int bitrate_kbps = kDefaultBitrateKbps;
};

struct RuntimeParams {
  base::TimeDelta duration = kDefaultDuration;
  double fps = 30.0;
  int max_frames = 100;
  base::FilePath frame_dir;
  base::FilePath chrome_path;

  // Fields that change per iteration (scenario/size)
  int frame_count = 0;
  webrtc::DesktopSize size;
  std::string scenario;
};

std::unique_ptr<WebrtcVideoEncoder> CreateEncoder(const EncoderParams& params) {
  std::unique_ptr<WebrtcVideoEncoder> encoder;
  if (params.codec == Codec::kAv1) {
    encoder = std::make_unique<WebrtcVideoEncoderAV1>();
  } else if (params.codec == Codec::kVp8) {
    encoder = WebrtcVideoEncoderVpx::CreateForVP8();
  } else if (params.codec == Codec::kVp9) {
    encoder = WebrtcVideoEncoderVpx::CreateForVP9();
  }

  if (encoder) {
    encoder->SetLosslessColor(params.profile == Profile::k1);
    encoder->SetUseActiveMap(params.use_active_map);
  }

  return encoder;
}

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
  EncoderBenchmark() {
    capture_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    encode_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  ~EncoderBenchmark() {
    // Flush both runners to ensure no use-after-free.
    base::RunLoop capture_loop;
    capture_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                           capture_loop.QuitClosure());
    capture_loop.Run();

    base::RunLoop encode_loop;
    encode_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::DestroyEncoderOnEncodeThread,
                       base::Unretained(this)),
        encode_loop.QuitClosure());
    encode_loop.Run();
  }

  void Run(const EncoderParams& encoder_params,
           const RuntimeParams& runtime_params) {
    encoder_params_ = encoder_params;
    runtime_params_ = runtime_params;
    frames_encoded_ = 0;
    total_pixels_ = 0;
    updated_pixels_ = 0;
    total_encode_time_ = base::TimeDelta();

    std::unique_ptr<FrameGenerator> generator;
    if (!runtime_params_.frame_dir.empty()) {
      generator = std::make_unique<FileFrameGenerator>(
          runtime_params_.frame_dir, runtime_params_.size);
    } else {
      auto headless = std::make_unique<HeadlessFrameGenerator>(
          runtime_params_.scenario, runtime_params_.size,
          runtime_params_.frame_count, runtime_params_.fps);
      if (!runtime_params_.chrome_path.empty()) {
        headless->SetChromePath(runtime_params_.chrome_path);
      }
      if (headless->Initialize()) {
        generator = std::move(headless);
      } else {
        LOG(ERROR) << "Failed to initialize headless frame generator.";
        return;
      }
    }

    // Wrap with differ to pre-calculate updated regions.
    generator = std::make_unique<DifferFrameGenerator>(std::move(generator));

    std::cout << "Pre-loading up to " << runtime_params_.frame_count
              << " frames..." << std::endl;
    preloaded_frames_.clear();
    for (int i = 0; i < runtime_params_.frame_count; ++i) {
      auto frame = generator->GenerateFrame();
      if (!frame) {
        break;
      }
      preloaded_frames_.push_back(std::move(frame));
    }

    if (preloaded_frames_.empty()) {
      LOG(ERROR) << "No frames generated.";
      return;
    }

    frames_to_encode_ = preloaded_frames_.size();

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

    encode_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::InitializeEncoderOnEncodeThread,
                       base::Unretained(this)));

    // Ensure encoder is initialized.
    base::RunLoop init_loop;
    encode_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                          init_loop.QuitClosure());
    init_loop.Run();

    if (!encoder_initialized_) {
      LOG(ERROR) << "Failed to initialize encoder for codec: "
                 << CodecToString(encoder_params_.codec);
      return;
    }

    std::cout << "Starting benchmark..." << std::endl;
    start_time_ = base::TimeTicks::Now();

    capture_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::CaptureNextFrameOnCaptureThread,
                       base::Unretained(this)));

    run_loop.Run();
    end_time_ = base::TimeTicks::Now();

    PrintResults();
  }

 private:
  void InitializeEncoderOnEncodeThread() {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    encoder_ = CreateEncoder(encoder_params_);
    encoder_initialized_ = (encoder_ != nullptr);
  }

  void DestroyEncoderOnEncodeThread() {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    encoder_.reset();
    encoder_initialized_ = false;
  }

  void CaptureNextFrameOnCaptureThread() {
    DCHECK(capture_task_runner_->RunsTasksInCurrentSequence());
    CHECK(!preloaded_frames_.empty());

    std::unique_ptr<webrtc::DesktopFrame> frame =
        std::move(preloaded_frames_.front());
    preloaded_frames_.pop_front();

    WebrtcVideoEncoder::FrameParams params;
    params.bitrate_kbps = encoder_params_.bitrate_kbps;
    params.duration = base::Hertz(runtime_params_.fps);
    params.key_frame = (frames_encoded_ == 0);
    params.clear_active_map = true;

    // Stats are tracked on capture thread.
    // Use int64_t to prevent overflow for large dimensions.
    total_pixels_ += static_cast<int64_t>(frame->size().width()) *
                     static_cast<int64_t>(frame->size().height());
    for (webrtc::DesktopRegion::Iterator r(frame->updated_region());
         !r.IsAtEnd(); r.Advance()) {
      updated_pixels_ += static_cast<int64_t>(r.rect().width()) *
                         static_cast<int64_t>(r.rect().height());
    }

    encode_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::EncodeFrameOnEncodeThread,
                       base::Unretained(this), std::move(frame), params));
  }

  void EncodeFrameOnEncodeThread(
      std::unique_ptr<webrtc::DesktopFrame> frame,
      const WebrtcVideoEncoder::FrameParams& params) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    if (!encoder_) {
      LOG(ERROR) << "Encoder was not initialized.";
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&EncoderBenchmark::OnFrameEncodedCompleteOnMainThread,
                         base::Unretained(this)));
      return;
    }
    encoder_->Encode(
        std::move(frame), params,
        base::BindOnce(&EncoderBenchmark::OnFrameEncodedOnEncodeThread,
                       base::Unretained(this), base::TimeTicks::Now()));
  }

  void OnFrameEncodedOnEncodeThread(
      base::TimeTicks start_time,
      WebrtcVideoEncoder::EncodeResult result,
      std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    if (result == WebrtcVideoEncoder::EncodeResult::SUCCEEDED && frame) {
      total_encode_time_ += base::TimeTicks::Now() - start_time;
    } else {
      LOG(ERROR) << "Encoder failed to encode frame.";
    }
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::OnFrameEncodedCompleteOnMainThread,
                       base::Unretained(this)));
  }

  void OnFrameEncodedCompleteOnMainThread() {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    frames_encoded_++;
    if (frames_encoded_ >= frames_to_encode_) {
      quit_closure_.Run();
    } else {
      capture_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&EncoderBenchmark::CaptureNextFrameOnCaptureThread,
                         base::Unretained(this)));
    }
  }

  void PrintResults() {
    std::cout << "--------------------------------------------------"
              << std::endl;
    std::cout << "RESULTS (" << CodecToString(encoder_params_.codec) << ", "
              << runtime_params_.size.width() << "x"
              << runtime_params_.size.height() << ", "
              << (runtime_params_.scenario.empty() ? "custom"
                                                   : runtime_params_.scenario)
              << ", " << runtime_params_.fps << " FPS)" << std::endl;
    std::cout << "Profile: "
              << (encoder_params_.profile == Profile::k1 ? "1" : "0") << " ("
              << (encoder_params_.color_space == ColorSpace::kI444 ? "I444"
                                                                  : "I420")
              << ")" << std::endl;
    std::cout << "Active Map: "
              << (encoder_params_.use_active_map ? "enabled" : "disabled")
              << std::endl;
    std::cout << "--------------------------------------------------"
              << std::endl;
    std::cout << "Encoded " << frames_encoded_ << " frames." << std::endl;

    double total_time_ms = (end_time_ - start_time_).InMillisecondsF();
    if (frames_encoded_ > 0 && total_time_ms > 0) {
      std::cout << "Total wall-clock time: " << total_time_ms << " ms"
                << std::endl;
      std::cout << "Average wall-clock FPS: "
                << (frames_encoded_ * 1000.0) / total_time_ms << std::endl;

      double total_encode_time_ms = total_encode_time_.InMillisecondsF();
      std::cout << "Total encoder-only time: " << total_encode_time_ms << " ms"
                << std::endl;
      std::cout << "Average encoder-only time per frame: "
                << total_encode_time_ms / frames_encoded_ << " ms" << std::endl;
      if (total_encode_time_ms > 0) {
        std::cout << "Average encoder FPS: "
                  << (frames_encoded_ * 1000.0) / total_encode_time_ms
                  << std::endl;
      } else {
        std::cout << "Average encoder FPS: N/A (zero encode time)" << std::endl;
      }
    } else {
      std::cout << "Average FPS: N/A (too few frames or zero time duration)"
                << std::endl;
    }

    if (total_pixels_ > 0) {
      std::cout << "Average updated area: "
                << (updated_pixels_ * 100.0) / total_pixels_ << "%"
                << std::endl;
    }
    std::cout << "--------------------------------------------------"
              << std::endl;
  }

  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::unique_ptr<WebrtcVideoEncoder> encoder_;
  bool encoder_initialized_ = false;

  EncoderParams encoder_params_;
  RuntimeParams runtime_params_;

  int frames_to_encode_ = 0;
  int frames_encoded_ = 0;
  int64_t total_pixels_ = 0;
  int64_t updated_pixels_ = 0;
  base::TimeDelta total_encode_time_;
  std::deque<std::unique_ptr<webrtc::DesktopFrame>> preloaded_frames_;
  base::RepeatingClosure quit_closure_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
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

  encoder_params.bitrate_kbps = kDefaultBitrateKbps;
  if (cmd_line->HasSwitch("bitrate")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("bitrate"),
                           &encoder_params.bitrate_kbps)) {
      LOG(ERROR) << "Invalid bitrate: "
                 << cmd_line->GetSwitchValueASCII("bitrate");
      return false;
    }
  }

  runtime_params.duration = kDefaultDuration;
  if (cmd_line->HasSwitch("duration")) {
    int duration_value;
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("duration"),
                           &duration_value)) {
      LOG(ERROR) << "Invalid duration: "
                 << cmd_line->GetSwitchValueASCII("duration");
      return false;
    }
    runtime_params.duration = base::Seconds(duration_value);
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
      base_runtime_params.duration.InSecondsF() * base_runtime_params.fps);
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
