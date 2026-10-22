// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
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
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

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
  std::optional<int> encoder_speed;
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

struct FrameMetric {
  double encode_time_ms = 0.0;
  size_t size_bytes = 0;
  int quantizer = 0;
  bool key_frame = false;
};

struct LatencyStats {
  size_t count = 0;
  double total_ms = 0.0;
  double mean_ms = 0.0;
  double p50_ms = 0.0;
  double p90_ms = 0.0;
  double p99_ms = 0.0;
  double max_ms = 0.0;
};

struct SizeStats {
  size_t count = 0;
  size_t total_bytes = 0;
  double avg_bytes = 0.0;
  size_t max_bytes = 0;
};

struct QuantizerStats {
  size_t count = 0;
  double avg_qp = 0.0;
  int min_qp = 0;
  int max_qp = 0;
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
    if (params.encoder_speed.has_value()) {
      encoder->SetEncoderSpeed(*params.encoder_speed);
    }
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

double ComputePercentileFromSorted(const std::vector<double>& sorted_values,
                                   double percentile) {
  if (sorted_values.empty()) {
    return 0.0;
  }
  double index = (percentile / 100.0) * (sorted_values.size() - 1);
  size_t lower = static_cast<size_t>(std::floor(index));
  size_t upper = static_cast<size_t>(std::ceil(index));
  double weight = index - lower;
  return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight;
}

LatencyStats ComputeLatencyStats(const std::vector<double>& times_ms) {
  LatencyStats stats;
  stats.count = times_ms.size();
  if (times_ms.empty()) {
    return stats;
  }
  std::vector<double> sorted_times = times_ms;
  std::ranges::sort(sorted_times);
  stats.total_ms =
      std::accumulate(sorted_times.begin(), sorted_times.end(), 0.0);
  stats.mean_ms = stats.total_ms / stats.count;
  stats.p50_ms = ComputePercentileFromSorted(sorted_times, 50.0);
  stats.p90_ms = ComputePercentileFromSorted(sorted_times, 90.0);
  stats.p99_ms = ComputePercentileFromSorted(sorted_times, 99.0);
  stats.max_ms = sorted_times.back();
  return stats;
}

SizeStats ComputeSizeStats(const std::vector<size_t>& sizes) {
  SizeStats stats;
  stats.count = sizes.size();
  if (sizes.empty()) {
    return stats;
  }
  stats.total_bytes =
      std::accumulate(sizes.begin(), sizes.end(), static_cast<size_t>(0));
  stats.avg_bytes = static_cast<double>(stats.total_bytes) / stats.count;
  stats.max_bytes = *std::ranges::max_element(sizes);
  return stats;
}

QuantizerStats ComputeQuantizerStats(const std::vector<int>& quantizers) {
  QuantizerStats stats;
  stats.count = quantizers.size();
  if (quantizers.empty()) {
    return stats;
  }
  int64_t sum =
      std::accumulate(quantizers.begin(), quantizers.end(), int64_t{0});
  stats.avg_qp = static_cast<double>(sum) / stats.count;
  auto [min_it, max_it] = std::ranges::minmax_element(quantizers);
  stats.min_qp = *min_it;
  stats.max_qp = *max_it;
  return stats;
}

std::vector<std::unique_ptr<webrtc::SharedDesktopFrame>> LoadFrames(
    const RuntimeParams& runtime_params) {
  std::unique_ptr<FrameGenerator> generator;
  if (!runtime_params.frame_dir.empty()) {
    generator = std::make_unique<FileFrameGenerator>(runtime_params.frame_dir,
                                                     runtime_params.size);
  } else {
    auto headless = std::make_unique<HeadlessFrameGenerator>(
        runtime_params.scenario, runtime_params.size,
        runtime_params.frame_count, runtime_params.fps);
    if (!runtime_params.chrome_path.empty()) {
      headless->SetChromePath(runtime_params.chrome_path);
    }
    if (headless->Initialize()) {
      generator = std::move(headless);
    } else {
      LOG(ERROR) << "Failed to initialize headless frame generator.";
      return {};
    }
  }

  // Wrap with differ to pre-calculate updated regions.
  generator = std::make_unique<DifferFrameGenerator>(std::move(generator));

  std::cout << "Pre-loading up to " << runtime_params.frame_count
            << " frames..." << std::endl;
  std::vector<std::unique_ptr<webrtc::SharedDesktopFrame>> frames;
  frames.reserve(runtime_params.frame_count);
  for (int i = 0; i < runtime_params.frame_count; ++i) {
    auto frame = generator->GenerateFrame();
    if (!frame) {
      break;
    }
    frames.push_back(webrtc::SharedDesktopFrame::Wrap(std::move(frame)));
  }

  return frames;
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
           const RuntimeParams& runtime_params,
           base::span<const std::unique_ptr<webrtc::SharedDesktopFrame>>
               preloaded_frames) {
    encoder_params_ = encoder_params;
    runtime_params_ = runtime_params;
    preloaded_frames_ = preloaded_frames;
    next_frame_index_ = 0;
    frames_encoded_ = 0;
    total_pixels_ = 0;
    updated_pixels_ = 0;
    frame_metrics_.clear();

    if (preloaded_frames.empty()) {
      LOG(ERROR) << "No frames available to encode.";
      return;
    }

    frames_to_encode_ = static_cast<int>(preloaded_frames.size());
    frame_metrics_.reserve(frames_to_encode_);

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
      preloaded_frames_ = {};
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

    // Destroy the encoder between runs to ensure clean state.
    base::RunLoop destroy_loop;
    encode_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::DestroyEncoderOnEncodeThread,
                       base::Unretained(this)),
        destroy_loop.QuitClosure());
    destroy_loop.Run();

    PrintResults();
    preloaded_frames_ = {};
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
    CHECK_LT(next_frame_index_, preloaded_frames_.size());

    size_t frame_index = next_frame_index_++;
    std::unique_ptr<webrtc::DesktopFrame> frame =
        preloaded_frames_[frame_index]->Share();

    WebrtcVideoEncoder::FrameParams params;
    params.bitrate_kbps = encoder_params_.bitrate_kbps;
    params.duration = base::Hertz(runtime_params_.fps);
    params.fps = static_cast<int>(std::round(runtime_params_.fps));
    params.key_frame = (frame_index == 0);
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
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    if (result == WebrtcVideoEncoder::EncodeResult::SUCCEEDED && frame) {
      FrameMetric metric;
      metric.encode_time_ms = elapsed.InMillisecondsF();
      metric.size_bytes = frame->data ? frame->data->size() : 0;
      metric.quantizer = frame->quantizer;
      metric.key_frame = frame->key_frame;
      frame_metrics_.push_back(metric);
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
    std::vector<double> all_times_ms;
    std::vector<double> keyframe_times_ms;
    std::vector<double> delta_times_ms;

    std::vector<size_t> all_sizes;
    std::vector<size_t> keyframe_sizes;
    std::vector<size_t> delta_sizes;

    std::vector<int> all_quantizers;
    std::vector<int> delta_quantizers;

    all_times_ms.reserve(frame_metrics_.size());
    all_sizes.reserve(frame_metrics_.size());
    all_quantizers.reserve(frame_metrics_.size());

    for (const auto& m : frame_metrics_) {
      all_times_ms.push_back(m.encode_time_ms);
      all_sizes.push_back(m.size_bytes);
      all_quantizers.push_back(m.quantizer);
      if (m.key_frame) {
        keyframe_times_ms.push_back(m.encode_time_ms);
        keyframe_sizes.push_back(m.size_bytes);
      } else {
        delta_times_ms.push_back(m.encode_time_ms);
        delta_sizes.push_back(m.size_bytes);
        delta_quantizers.push_back(m.quantizer);
      }
    }

    LatencyStats all_latency = ComputeLatencyStats(all_times_ms);
    LatencyStats kf_latency = ComputeLatencyStats(keyframe_times_ms);
    LatencyStats delta_latency = ComputeLatencyStats(delta_times_ms);

    SizeStats all_size_stats = ComputeSizeStats(all_sizes);
    SizeStats kf_size_stats = ComputeSizeStats(keyframe_sizes);
    SizeStats delta_size_stats = ComputeSizeStats(delta_sizes);

    QuantizerStats all_qp_stats = ComputeQuantizerStats(all_quantizers);
    QuantizerStats delta_qp_stats = ComputeQuantizerStats(delta_quantizers);

    double wall_clock_ms = (end_time_ - start_time_).InMillisecondsF();
    double wall_clock_fps =
        (wall_clock_ms > 0.0 && !frame_metrics_.empty())
            ? (frame_metrics_.size() * 1000.0) / wall_clock_ms
            : 0.0;
    double encoder_fps =
        (all_latency.total_ms > 0.0 && !frame_metrics_.empty())
            ? (frame_metrics_.size() * 1000.0) / all_latency.total_ms
            : 0.0;
    double updated_area_pct =
        (total_pixels_ > 0) ? (updated_pixels_ * 100.0) / total_pixels_ : 0.0;
    double achieved_bitrate_kbps =
        (!frame_metrics_.empty() && runtime_params_.fps > 0.0)
            ? (static_cast<double>(all_size_stats.total_bytes) * 8.0 *
               runtime_params_.fps) /
                  (frame_metrics_.size() * 1000.0)
            : 0.0;

    std::ios old_state(nullptr);
    old_state.copyfmt(std::cout);
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "=================================================="
              << std::endl;
    std::cout << "RESULTS (" << CodecToString(encoder_params_.codec) << ", "
              << runtime_params_.size.width() << "x"
              << runtime_params_.size.height() << ", "
              << (runtime_params_.scenario.empty() ? "custom"
                                                   : runtime_params_.scenario)
              << ", " << runtime_params_.fps << " FPS)" << std::endl;
    std::cout << "Config:  Profile "
              << (encoder_params_.profile == Profile::k1 ? "1 (I444)"
                                                         : "0 (I420)")
              << " | Active Map: "
              << (encoder_params_.use_active_map ? "enabled" : "disabled")
              << " | Target Bitrate: " << encoder_params_.bitrate_kbps
              << " kbps" << std::endl;
    std::cout << "--------------------------------------------------"
              << std::endl;
    std::cout << "Frames encoded:           " << frame_metrics_.size() << " ("
              << kf_latency.count << " key, " << delta_latency.count
              << " delta)" << std::endl;
    std::cout << "Average updated area:     " << updated_area_pct << "%"
              << std::endl;
    std::cout << "Wall-clock time:          " << wall_clock_ms << " ms ("
              << wall_clock_fps << " FPS)" << std::endl;
    std::cout << "Encoder-only time:        " << all_latency.total_ms << " ms ("
              << encoder_fps << " FPS)" << std::endl;
    std::cout << "--------------------------------------------------"
              << std::endl;
    std::cout << "Encode Latency (All):     mean=" << all_latency.mean_ms
              << " ms | p50=" << all_latency.p50_ms
              << " ms | p90=" << all_latency.p90_ms
              << " ms | p99=" << all_latency.p99_ms
              << " ms | max=" << all_latency.max_ms << " ms" << std::endl;
    if (kf_latency.count > 0) {
      std::cout << "Encode Latency (Key):     mean=" << kf_latency.mean_ms
                << " ms | max=" << kf_latency.max_ms << " ms" << std::endl;
    }
    if (delta_latency.count > 0) {
      std::cout << "Encode Latency (Delta):   mean=" << delta_latency.mean_ms
                << " ms | p50=" << delta_latency.p50_ms
                << " ms | p90=" << delta_latency.p90_ms
                << " ms | p99=" << delta_latency.p99_ms
                << " ms | max=" << delta_latency.max_ms << " ms" << std::endl;
    }
    std::cout << "--------------------------------------------------"
              << std::endl;
    std::cout << "Stream Bitrate:           " << achieved_bitrate_kbps
              << " kbps (target: " << encoder_params_.bitrate_kbps << " kbps)"
              << std::endl;
    std::cout << "Total Encoded Size:       " << all_size_stats.total_bytes
              << " bytes (" << (all_size_stats.total_bytes / 1024.0) << " KiB)"
              << std::endl;
    std::cout << "Frame Size (All):         avg=" << all_size_stats.avg_bytes
              << " B | max=" << all_size_stats.max_bytes << " B" << std::endl;
    if (kf_size_stats.count > 0) {
      std::cout << "Frame Size (Key):         avg=" << kf_size_stats.avg_bytes
                << " B | max=" << kf_size_stats.max_bytes << " B" << std::endl;
    }
    if (delta_size_stats.count > 0) {
      std::cout << "Frame Size (Delta):       avg="
                << delta_size_stats.avg_bytes
                << " B | max=" << delta_size_stats.max_bytes << " B"
                << std::endl;
    }
    if (all_qp_stats.count > 0) {
      std::cout << "Quantizer / QP (All):     avg=" << all_qp_stats.avg_qp
                << " | min=" << all_qp_stats.min_qp
                << " | max=" << all_qp_stats.max_qp << std::endl;
    }
    if (delta_qp_stats.count > 0) {
      std::cout << "Quantizer / QP (Delta):   avg=" << delta_qp_stats.avg_qp
                << " | min=" << delta_qp_stats.min_qp
                << " | max=" << delta_qp_stats.max_qp << std::endl;
    }
    std::cout << "=================================================="
              << std::endl;

    std::cout.copyfmt(old_state);
  }

  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::unique_ptr<WebrtcVideoEncoder> encoder_;
  bool encoder_initialized_ = false;

  EncoderParams encoder_params_;
  RuntimeParams runtime_params_;

  base::raw_span<const std::unique_ptr<webrtc::SharedDesktopFrame>>
      preloaded_frames_;
  size_t next_frame_index_ = 0;
  int frames_to_encode_ = 0;
  int frames_encoded_ = 0;
  int64_t total_pixels_ = 0;
  int64_t updated_pixels_ = 0;
  std::vector<FrameMetric> frame_metrics_;
  base::RepeatingClosure quit_closure_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
};

void PrintHelp(const char* executable_name) {
  std::cout << "Usage: " << executable_name << R"([options]
Options:
  --codec=<av1|vp8|vp9>  Codec to benchmark (default: av1)
  --profile=<0|1>        Encoder profile (default: 0)
  --encoder-speed=<N>    Encoder speed override
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

  if (cmd_line->HasSwitch("encoder-speed")) {
    int speed_val = 0;
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("encoder-speed"),
                           &speed_val)) {
      LOG(ERROR) << "Invalid encoder-speed: "
                 << cmd_line->GetSwitchValueASCII("encoder-speed");
      return false;
    }
    encoder_params.encoder_speed = speed_val;
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

      auto preloaded_frames = remoting::LoadFrames(runtime_params);
      if (preloaded_frames.empty()) {
        LOG(ERROR) << "No frames generated.";
        continue;
      }

      benchmark.Run(encoder_params, runtime_params, preloaded_frames);
    }
  }

  return 0;
}
