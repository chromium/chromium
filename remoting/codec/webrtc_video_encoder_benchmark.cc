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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "remoting/codec/webrtc_video_encoder_av1.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/test/frame_generator/differ_frame_generator.h"
#include "remoting/test/frame_generator/file_frame_generator.h"
#include "remoting/test/frame_generator/headless_frame_generator.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

namespace {

constexpr int kDefaultFrames = 100;
constexpr double kDefaultFps = 30.0;
constexpr int kDefaultTargetBitrateKbpsPerMegapixel = 2500;
constexpr int kMinimumDefaultBitrateKbps = 1000;
constexpr int kMinDimension = 32;
constexpr int kMinQuantizer = 10;

constexpr int kAv1DefaultEncoderSpeed = 11;
constexpr int kAv1MinEncoderSpeed = 7;
constexpr int kAv1MaxEncoderSpeed = 11;

constexpr int kVp9DefaultEncoderSpeed = 9;
constexpr int kVp9MinEncoderSpeed = 5;
constexpr int kVp9MaxEncoderSpeed = 9;

constexpr int kVp8FixedEncoderSpeed = 16;

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

struct EncoderConfig {
  std::string name;
  Codec codec = Codec::kAv1;
  Profile profile = Profile::k0;
  ColorSpace color_space = ColorSpace::kI420;
  int encoder_speed = kAv1DefaultEncoderSpeed;
  bool use_active_map = false;
  int bitrate_kbps = 0;
};

struct BenchmarkConfig {
  int frames = kDefaultFrames;
  double fps = kDefaultFps;
  int bitrate_kbps = 0;
  std::vector<webrtc::DesktopSize> sizes;
  std::vector<std::string> scenarios;
  std::vector<EncoderConfig> encoders;
  base::FilePath frame_dir;
  base::FilePath chrome_path;
  base::FilePath output_json_path;
};

struct RuntimeParams {
  double fps = kDefaultFps;
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

std::string DefaultEncoderName(const EncoderConfig& config) {
  int profile_num = (config.profile == Profile::k1) ? 1 : 0;
  switch (config.codec) {
    case Codec::kAv1:
      return base::StringPrintf("av1_p%d_s%d_%s", profile_num,
                                config.encoder_speed,
                                config.use_active_map ? "am" : "no_am");
    case Codec::kVp9:
      return base::StringPrintf("vp9_p%d_s%d", profile_num,
                                config.encoder_speed);
    case Codec::kVp8:
      return "vp8_p0";
  }
}

// Returns the target bitrate (in kbps) for a benchmark run. If an explicit
// bitrate was configured (via the top-level or per-encoder JSON "bitrate_kbps"
// field), that value is used directly. Otherwise, the default target bitrate is
// scaled linearly with the frame resolution at 2500 kbps per megapixel (e.g.,
// ~5.18 Mbps for 1080p, ~9.22 Mbps for 1440p, ~20.74 Mbps for 4K), with a floor
// of 1000 kbps for sub-megapixel resolutions. This matches the 2500 kbps/MP
// density used by WebrtcVideoEncoderVpx so that multi-resolution runs (1080p,
// 1440p, 4K) maintain a consistent bits-per-pixel budget rather than starving
// higher resolutions at a fixed 1080p bitrate.
int GetTargetBitrateKbps(int configured_bitrate_kbps,
                         const webrtc::DesktopSize& size) {
  if (configured_bitrate_kbps > 0) {
    return configured_bitrate_kbps;
  }
  constexpr int64_t kPixelsPerMegapixel = 1000000;
  int64_t pixels =
      static_cast<int64_t>(size.width()) * static_cast<int64_t>(size.height());
  int scaled_bitrate_kbps = static_cast<int>(
      (pixels * kDefaultTargetBitrateKbpsPerMegapixel) / kPixelsPerMegapixel);
  return std::max(kMinimumDefaultBitrateKbps, scaled_bitrate_kbps);
}

std::unique_ptr<WebrtcVideoEncoder> CreateEncoder(const EncoderConfig& config) {
  std::unique_ptr<WebrtcVideoEncoder> encoder;
  switch (config.codec) {
    case Codec::kAv1:
      encoder = std::make_unique<WebrtcVideoEncoderAV1>();
      break;
    case Codec::kVp8:
      encoder = WebrtcVideoEncoderVpx::CreateForVP8();
      break;
    case Codec::kVp9:
      encoder = WebrtcVideoEncoderVpx::CreateForVP9();
      break;
  }

  if (encoder) {
    encoder->SetLosslessColor(config.profile == Profile::k1);
    if (config.codec == Codec::kAv1) {
      encoder->SetUseActiveMap(config.use_active_map);
      encoder->SetEncoderSpeed(config.encoder_speed);
    } else if (config.codec == Codec::kVp9) {
      encoder->SetEncoderSpeed(config.encoder_speed);
    }
  }

  return encoder;
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
    const BenchmarkConfig& config,
    const std::string& scenario,
    const webrtc::DesktopSize& size) {
  std::unique_ptr<FrameGenerator> generator;
  if (!config.frame_dir.empty()) {
    generator = std::make_unique<FileFrameGenerator>(config.frame_dir, size);
  } else {
    auto headless = std::make_unique<HeadlessFrameGenerator>(
        scenario, size, config.frames, config.fps);
    if (!config.chrome_path.empty()) {
      headless->SetChromePath(config.chrome_path);
    }
    if (!headless->Initialize()) {
      LOG(ERROR) << "Failed to initialize headless frame generator.";
      return {};
    }
    generator = std::move(headless);
  }

  // Wrap with differ to pre-calculate updated regions.
  generator = std::make_unique<DifferFrameGenerator>(std::move(generator));

  std::cout << "Pre-loading up to " << config.frames << " frames for scenario '"
            << scenario << "' at " << size.width() << "x" << size.height()
            << "..." << std::endl;

  std::vector<std::unique_ptr<webrtc::SharedDesktopFrame>> frames;
  frames.reserve(config.frames);
  for (int i = 0; i < config.frames; ++i) {
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

  base::DictValue Run(
      const EncoderConfig& encoder_config,
      const RuntimeParams& runtime_params,
      base::span<const std::unique_ptr<webrtc::SharedDesktopFrame>>
          preloaded_frames) {
    encoder_config_ = encoder_config;
    runtime_params_ = runtime_params;
    encoder_config_.bitrate_kbps = GetTargetBitrateKbps(
        encoder_config_.bitrate_kbps, runtime_params_.size);
    preloaded_frames_ = preloaded_frames;
    next_frame_index_ = 0;
    frames_encoded_ = 0;
    top_off_active_ = false;
    total_pixels_ = 0;
    updated_pixels_ = 0;
    frame_metrics_.clear();
    frame_metrics_.reserve(preloaded_frames.size());

    if (preloaded_frames.empty()) {
      LOG(ERROR) << "No frames available to encode.";
      return base::DictValue();
    }

    frames_to_encode_ = static_cast<int>(preloaded_frames.size());

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

    encode_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::InitializeEncoderOnEncodeThread,
                       base::Unretained(this)));

    // Ensure encoder is initialized before starting the timer.
    base::RunLoop init_loop;
    encode_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                          init_loop.QuitClosure());
    init_loop.Run();

    if (!encoder_initialized_) {
      LOG(ERROR) << "Failed to initialize encoder: " << encoder_config_.name;
      preloaded_frames_ = {};
      return base::DictValue();
    }

    std::cout << "Running encoder '" << encoder_config_.name << "'..."
              << std::endl;
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

    preloaded_frames_ = {};
    return SummarizeAndPrintResults();
  }

 private:
  void InitializeEncoderOnEncodeThread() {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    auto warmup_encoder = CreateEncoder(encoder_config_);
    if (!warmup_encoder) {
      encoder_initialized_ = false;
      return;
    }

    // Run a single warm-up encode on a temporary instance to warm up the
    // encode thread, SIMD dispatch tables, and allocator caches without
    // mutating the rate-control state of |encoder_|.
    WebrtcVideoEncoder::FrameParams warmup_params;
    warmup_params.bitrate_kbps = encoder_config_.bitrate_kbps;
    warmup_params.duration = base::Hertz(runtime_params_.fps);
    warmup_params.fps = static_cast<int>(std::round(runtime_params_.fps));
    warmup_params.key_frame = true;
    warmup_params.clear_active_map = true;
    warmup_encoder->Encode(preloaded_frames_[0]->Share(), warmup_params,
                           base::DoNothing());
    warmup_encoder.reset();

    encoder_ = CreateEncoder(encoder_config_);
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
    params.bitrate_kbps = encoder_config_.bitrate_kbps;
    params.duration = base::Hertz(runtime_params_.fps);
    params.fps = static_cast<int>(std::round(runtime_params_.fps));
    params.key_frame = (frame_index == 0);
    params.clear_active_map = !top_off_active_;

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
      base::TimeTicks encode_start_time,
      WebrtcVideoEncoder::EncodeResult result,
      std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    base::TimeDelta elapsed = base::TimeTicks::Now() - encode_start_time;
    if (result == WebrtcVideoEncoder::EncodeResult::SUCCEEDED && frame) {
      FrameMetric metric;
      metric.encode_time_ms = elapsed.InMillisecondsF();
      metric.size_bytes = frame->data ? frame->data->size() : 0;
      metric.quantizer = frame->quantizer;
      metric.key_frame = frame->key_frame;
      frame_metrics_.push_back(metric);
      top_off_active_ = (frame->quantizer > kMinQuantizer);
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

  base::DictValue SummarizeAndPrintResults() {
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
    std::cout << "RESULTS: " << encoder_config_.name << " ("
              << CodecToString(encoder_config_.codec) << ", "
              << runtime_params_.size.width() << "x"
              << runtime_params_.size.height() << ", "
              << runtime_params_.scenario << ", " << runtime_params_.fps
              << " FPS)" << std::endl;
    std::cout << "Config:  Profile "
              << (encoder_config_.profile == Profile::k1 ? "1 (I444)"
                                                         : "0 (I420)")
              << " | Speed " << encoder_config_.encoder_speed
              << " | Active Map: "
              << (encoder_config_.use_active_map ? "enabled" : "disabled")
              << " | Target Bitrate: " << encoder_config_.bitrate_kbps
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
              << " kbps (target: " << encoder_config_.bitrate_kbps << " kbps)"
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

    base::DictValue result;
    result.Set("encoder_name", encoder_config_.name);
    result.Set("codec", CodecToString(encoder_config_.codec));
    result.Set("profile", encoder_config_.profile == Profile::k1 ? 1 : 0);
    result.Set("color_space", encoder_config_.color_space == ColorSpace::kI444
                                  ? "I444"
                                  : "I420");
    result.Set("encoder_speed", encoder_config_.encoder_speed);
    result.Set("use_active_map", encoder_config_.use_active_map);
    result.Set("target_bitrate_kbps", encoder_config_.bitrate_kbps);
    result.Set("scenario", runtime_params_.scenario);
    result.Set("width", runtime_params_.size.width());
    result.Set("height", runtime_params_.size.height());
    result.Set("target_fps", runtime_params_.fps);

    result.Set("frames_encoded", static_cast<int>(frame_metrics_.size()));
    result.Set("keyframes_encoded", static_cast<int>(kf_latency.count));
    result.Set("delta_frames_encoded", static_cast<int>(delta_latency.count));
    result.Set("updated_area_percent", updated_area_pct);
    result.Set("wall_clock_time_ms", wall_clock_ms);
    result.Set("wall_clock_fps", wall_clock_fps);
    result.Set("encoder_time_ms", all_latency.total_ms);
    result.Set("encoder_fps", encoder_fps);
    result.Set("achieved_bitrate_kbps", achieved_bitrate_kbps);
    result.Set("total_encoded_bytes",
               static_cast<double>(all_size_stats.total_bytes));

    auto latency_to_dict = [](const LatencyStats& s) {
      base::DictValue d;
      d.Set("count", static_cast<int>(s.count));
      d.Set("total_ms", s.total_ms);
      d.Set("mean_ms", s.mean_ms);
      d.Set("p50_ms", s.p50_ms);
      d.Set("p90_ms", s.p90_ms);
      d.Set("p99_ms", s.p99_ms);
      d.Set("max_ms", s.max_ms);
      return d;
    };
    result.Set("latency_all", latency_to_dict(all_latency));
    result.Set("latency_keyframe", latency_to_dict(kf_latency));
    result.Set("latency_delta", latency_to_dict(delta_latency));

    auto size_to_dict = [](const SizeStats& s) {
      base::DictValue d;
      d.Set("count", static_cast<int>(s.count));
      d.Set("total_bytes", static_cast<double>(s.total_bytes));
      d.Set("avg_bytes", s.avg_bytes);
      d.Set("max_bytes", static_cast<double>(s.max_bytes));
      return d;
    };
    result.Set("frame_size_all", size_to_dict(all_size_stats));
    result.Set("frame_size_keyframe", size_to_dict(kf_size_stats));
    result.Set("frame_size_delta", size_to_dict(delta_size_stats));

    auto qp_to_dict = [](const QuantizerStats& s) {
      base::DictValue d;
      d.Set("count", static_cast<int>(s.count));
      d.Set("avg_qp", s.avg_qp);
      d.Set("min_qp", s.min_qp);
      d.Set("max_qp", s.max_qp);
      return d;
    };
    result.Set("quantizer_all", qp_to_dict(all_qp_stats));
    result.Set("quantizer_delta", qp_to_dict(delta_qp_stats));

    return result;
  }

  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::unique_ptr<WebrtcVideoEncoder> encoder_;
  bool encoder_initialized_ = false;

  EncoderConfig encoder_config_;
  RuntimeParams runtime_params_;

  base::raw_span<const std::unique_ptr<webrtc::SharedDesktopFrame>>
      preloaded_frames_;
  size_t next_frame_index_ = 0;
  int frames_to_encode_ = 0;
  int frames_encoded_ = 0;
  bool top_off_active_ = false;
  int64_t total_pixels_ = 0;
  int64_t updated_pixels_ = 0;
  std::vector<FrameMetric> frame_metrics_;
  base::RepeatingClosure quit_closure_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
};

void PrintHelp(const char* executable_name) {
  std::cout << "Usage: " << executable_name << R"( [options]

Options:
  --config=<path>        Path to JSON benchmark configuration file
  --chrome-path=<path>   Override path to the Chrome/Chromium binary
  --frame-dir=<path>     Override path to pre-generated PNG frames
  --output-json=<path>   Write benchmark results to the specified JSON file
  --help                 Print this help message and JSON schema

Example JSON configuration:
{
  "frames": 100,
  "fps": 30.0,
  "bitrate_kbps": 5000,
  "sizes": ["1920x1080", "2560x1440", "3840x2160"],
  "scenarios": ["desktop_clock", "scroll_vertical"],
  "encoders": [
    {
      "name": "av1_s11_no_am",
      "codec": "av1",
      "profile": 0,
      "encoder_speed": 11,
      "use_active_map": false
    },
    {
      "name": "vp9_s9_i420",
      "codec": "vp9",
      "profile": 0,
      "encoder_speed": 9
    },
    {
      "name": "vp8_default",
      "codec": "vp8"
    }
  ]
}
)";
}

std::vector<std::string> GetDefaultScenarios() {
  return {"desktop_clock", "scroll_vertical", "scroll_horizontal",
          "moving_window", "busy_developer",  "low_motion",
          "high_motion",   "chaos_stress"};
}

std::vector<webrtc::DesktopSize> GetDefaultSizes() {
  return {{1920, 1080}, {2560, 1440}, {3840, 2160}};
}

bool ParseSizeWxH(std::string_view size_str, webrtc::DesktopSize& out_size) {
  std::vector<std::string_view> dims = base::SplitStringPiece(
      size_str, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (dims.size() != 2) {
    LOG(ERROR) << "Invalid size '" << size_str
               << "'. Expected 'WxH' format (e.g., '1920x1080').";
    return false;
  }
  int width = 0;
  int height = 0;
  if (!base::StringToInt(dims[0], &width) ||
      !base::StringToInt(dims[1], &height) || width < kMinDimension ||
      height < kMinDimension) {
    LOG(ERROR) << "Invalid size dimensions '" << size_str
               << "'. Width and height must be integers >= " << kMinDimension
               << ".";
    return false;
  }
  out_size = webrtc::DesktopSize(width, height);
  return true;
}

bool ParseEncoderConfig(const base::DictValue& dict,
                        int default_bitrate_kbps,
                        EncoderConfig& out_config) {
  static constexpr auto kAllowedKeys = base::MakeFixedFlatSet<std::string_view>(
      {"name", "codec", "profile", "encoder_speed", "use_active_map",
       "bitrate_kbps"});
  for (const auto [key, _] : dict) {
    if (!kAllowedKeys.contains(key)) {
      LOG(ERROR) << "Unknown key '" << key << "' in encoder configuration.";
      return false;
    }
  }

  const std::string* codec_ptr = dict.FindString("codec");
  if (!codec_ptr) {
    LOG(ERROR) << "Encoder configuration is missing required 'codec' string.";
    return false;
  }
  std::string codec_str = base::ToLowerASCII(*codec_ptr);
  if (codec_str == "av1") {
    out_config.codec = Codec::kAv1;
  } else if (codec_str == "vp8") {
    out_config.codec = Codec::kVp8;
  } else if (codec_str == "vp9") {
    out_config.codec = Codec::kVp9;
  } else {
    LOG(ERROR) << "Invalid codec '" << *codec_ptr
               << "'. Supported codecs: av1, vp8, vp9.";
    return false;
  }

  int profile_val = 0;
  if (const base::Value* profile_value = dict.Find("profile")) {
    if (!profile_value->is_int()) {
      LOG(ERROR) << "'profile' must be an integer (0 or 1).";
      return false;
    }
    profile_val = profile_value->GetInt();
    if (profile_val != 0 && profile_val != 1) {
      LOG(ERROR) << "Invalid profile " << profile_val
                 << ". Only 0 and 1 are supported.";
      return false;
    }
  }
  if (out_config.codec == Codec::kVp8 && profile_val != 0) {
    LOG(ERROR) << "VP8 only supports profile 0.";
    return false;
  }
  out_config.profile = (profile_val == 1) ? Profile::k1 : Profile::k0;
  out_config.color_space = (out_config.profile == Profile::k1)
                               ? ColorSpace::kI444
                               : ColorSpace::kI420;

  if (const base::Value* speed_value = dict.Find("encoder_speed")) {
    if (!speed_value->is_int()) {
      LOG(ERROR) << "'encoder_speed' must be an integer.";
      return false;
    }
    int speed = speed_value->GetInt();
    if (out_config.codec == Codec::kAv1) {
      if (speed < kAv1MinEncoderSpeed || speed > kAv1MaxEncoderSpeed) {
        LOG(ERROR) << "AV1 encoder_speed must be between "
                   << kAv1MinEncoderSpeed << " and " << kAv1MaxEncoderSpeed
                   << " (got " << speed << ").";
        return false;
      }
      out_config.encoder_speed = speed;
    } else if (out_config.codec == Codec::kVp9) {
      if (speed < kVp9MinEncoderSpeed || speed > kVp9MaxEncoderSpeed) {
        LOG(ERROR) << "VP9 encoder_speed must be between "
                   << kVp9MinEncoderSpeed << " and " << kVp9MaxEncoderSpeed
                   << " (got " << speed << ").";
        return false;
      }
      out_config.encoder_speed = speed;
    } else {
      LOG(ERROR) << "VP8 does not support configurable 'encoder_speed'.";
      return false;
    }
  } else {
    switch (out_config.codec) {
      case Codec::kAv1:
        out_config.encoder_speed = kAv1DefaultEncoderSpeed;
        break;
      case Codec::kVp9:
        out_config.encoder_speed = kVp9DefaultEncoderSpeed;
        break;
      case Codec::kVp8:
        out_config.encoder_speed = kVp8FixedEncoderSpeed;
        break;
    }
  }

  if (const base::Value* active_map_value = dict.Find("use_active_map")) {
    if (!active_map_value->is_bool()) {
      LOG(ERROR) << "'use_active_map' must be a boolean.";
      return false;
    }
    if (out_config.codec != Codec::kAv1) {
      LOG(ERROR) << "'use_active_map' can only be configured for AV1 (always "
                    "enabled for VP8 and VP9).";
      return false;
    }
    out_config.use_active_map = active_map_value->GetBool();
  } else {
    out_config.use_active_map = (out_config.codec != Codec::kAv1);
  }

  out_config.bitrate_kbps = default_bitrate_kbps;
  if (const base::Value* bitrate_value = dict.Find("bitrate_kbps")) {
    if (!bitrate_value->is_int() || bitrate_value->GetInt() <= 0) {
      LOG(ERROR) << "'bitrate_kbps' must be a positive integer.";
      return false;
    }
    out_config.bitrate_kbps = bitrate_value->GetInt();
  }

  if (const std::string* name_ptr = dict.FindString("name")) {
    if (name_ptr->empty()) {
      LOG(ERROR) << "'name' cannot be empty when specified.";
      return false;
    }
    out_config.name = *name_ptr;
  } else {
    out_config.name = DefaultEncoderName(out_config);
  }

  return true;
}

bool LoadJsonConfigFile(const base::FilePath& config_path,
                        BenchmarkConfig& config) {
  std::string json_content;
  if (!base::ReadFileToString(config_path, &json_content)) {
    LOG(ERROR) << "Failed to read config file: " << config_path;
    return false;
  }

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(
      json_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed.has_value()) {
    LOG(ERROR) << "Failed to parse JSON config " << config_path << " (line "
               << parsed.error().line << ", col " << parsed.error().column
               << "): " << parsed.error().message;
    return false;
  }
  if (!parsed->is_dict()) {
    LOG(ERROR) << "Root of JSON config must be an object.";
    return false;
  }

  const base::DictValue& root = parsed->GetDict();
  static constexpr auto kAllowedTopKeys =
      base::MakeFixedFlatSet<std::string_view>(
          {"frames", "fps", "bitrate_kbps", "sizes", "scenarios", "encoders",
           "frame_dir", "chrome_path", "output_json"});
  for (const auto [key, _] : root) {
    if (!kAllowedTopKeys.contains(key)) {
      LOG(ERROR) << "Unknown top-level key '" << key << "' in JSON config.";
      return false;
    }
  }

  if (const base::Value* frames_val = root.Find("frames")) {
    if (!frames_val->is_int() || frames_val->GetInt() <= 0) {
      LOG(ERROR) << "'frames' must be a positive integer.";
      return false;
    }
    config.frames = frames_val->GetInt();
  }

  if (const base::Value* fps_val = root.Find("fps")) {
    if ((!fps_val->is_double() && !fps_val->is_int()) ||
        fps_val->GetDouble() <= 0.0) {
      LOG(ERROR) << "'fps' must be a positive number.";
      return false;
    }
    config.fps = fps_val->GetDouble();
  }

  if (const base::Value* bitrate_val = root.Find("bitrate_kbps")) {
    if (!bitrate_val->is_int() || bitrate_val->GetInt() <= 0) {
      LOG(ERROR) << "'bitrate_kbps' must be a positive integer.";
      return false;
    }
    config.bitrate_kbps = bitrate_val->GetInt();
  }

  if (const std::string* frame_dir = root.FindString("frame_dir")) {
    config.frame_dir = base::FilePath::FromUTF8Unsafe(*frame_dir);
  }

  if (const std::string* chrome_path = root.FindString("chrome_path")) {
    config.chrome_path = base::FilePath::FromUTF8Unsafe(*chrome_path);
  }

  if (const std::string* output_json = root.FindString("output_json")) {
    config.output_json_path = base::FilePath::FromUTF8Unsafe(*output_json);
  }

  if (const base::Value* sizes_val = root.Find("sizes")) {
    if (!sizes_val->is_list() || sizes_val->GetList().empty()) {
      LOG(ERROR) << "'sizes' must be a non-empty array of 'WxH' strings.";
      return false;
    }
    config.sizes.clear();
    for (const base::Value& item : sizes_val->GetList()) {
      if (!item.is_string()) {
        LOG(ERROR) << "Each entry in 'sizes' must be a 'WxH' string.";
        return false;
      }
      webrtc::DesktopSize parsed_size;
      if (!ParseSizeWxH(item.GetString(), parsed_size)) {
        return false;
      }
      config.sizes.push_back(parsed_size);
    }
  }

  if (const base::Value* scenarios_val = root.Find("scenarios")) {
    if (!scenarios_val->is_list() || scenarios_val->GetList().empty()) {
      LOG(ERROR) << "'scenarios' must be a non-empty array of strings.";
      return false;
    }
    config.scenarios.clear();
    for (const base::Value& item : scenarios_val->GetList()) {
      if (!item.is_string() || item.GetString().empty()) {
        LOG(ERROR) << "Each entry in 'scenarios' must be a non-empty string.";
        return false;
      }
      if (item.GetString() == "all") {
        config.scenarios = GetDefaultScenarios();
        break;
      }
      config.scenarios.push_back(item.GetString());
    }
  }

  if (const base::Value* encoders_val = root.Find("encoders")) {
    if (!encoders_val->is_list() || encoders_val->GetList().empty()) {
      LOG(ERROR) << "'encoders' must be a non-empty array of encoder objects.";
      return false;
    }
    config.encoders.clear();
    for (const base::Value& item : encoders_val->GetList()) {
      if (!item.is_dict()) {
        LOG(ERROR) << "Each entry in 'encoders' must be a JSON object.";
        return false;
      }
      EncoderConfig encoder_config;
      if (!ParseEncoderConfig(item.GetDict(), config.bitrate_kbps,
                              encoder_config)) {
        return false;
      }
      config.encoders.push_back(std::move(encoder_config));
    }
  } else if (config.bitrate_kbps > 0) {
    for (auto& encoder_config : config.encoders) {
      encoder_config.bitrate_kbps = config.bitrate_kbps;
    }
  }

  return true;
}

bool InitializeBenchmarkConfig(const base::CommandLine* cmd_line,
                               BenchmarkConfig& config) {
  static constexpr const char* kRemovedSwitches[] = {
      "codec",    "profile", "encoder-speed", "use-active-map", "bitrate",
      "duration", "fps",     "max-frames",    "scenarios",      "sizes"};
  for (const char* removed : kRemovedSwitches) {
    if (cmd_line->HasSwitch(removed)) {
      LOG(ERROR) << "--" << removed
                 << " is no longer supported on the command line. Provide a "
                    "JSON config file via --config=<path> (see --help).";
      return false;
    }
  }

  config.sizes = GetDefaultSizes();
  config.scenarios = GetDefaultScenarios();
  EncoderConfig default_encoder;
  default_encoder.name = DefaultEncoderName(default_encoder);
  config.encoders = {default_encoder};

  if (cmd_line->HasSwitch("config")) {
    base::FilePath config_path = cmd_line->GetSwitchValuePath("config");
    if (!LoadJsonConfigFile(config_path, config)) {
      return false;
    }
  }

  if (cmd_line->HasSwitch("frame-dir")) {
    config.frame_dir = cmd_line->GetSwitchValuePath("frame-dir");
  }
  if (cmd_line->HasSwitch("chrome-path")) {
    config.chrome_path = cmd_line->GetSwitchValuePath("chrome-path");
  }
  if (cmd_line->HasSwitch("output-json")) {
    config.output_json_path = cmd_line->GetSwitchValuePath("output-json");
  }

  if (!config.frame_dir.empty()) {
    config.scenarios = {"user_provided_frames"};
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

  remoting::BenchmarkConfig config;
  if (!remoting::InitializeBenchmarkConfig(cmd_line, config)) {
    return 1;
  }

  remoting::EncoderBenchmark benchmark;
  base::ListValue all_results;

  for (const auto& scenario : config.scenarios) {
    for (const auto& size : config.sizes) {
      auto preloaded_frames = remoting::LoadFrames(config, scenario, size);
      if (preloaded_frames.empty()) {
        LOG(ERROR) << "No frames generated for scenario '" << scenario
                   << "' at " << size.width() << "x" << size.height() << ".";
        continue;
      }

      remoting::RuntimeParams runtime_params;
      runtime_params.fps = config.fps;
      runtime_params.size = size;
      runtime_params.scenario = scenario;

      for (const auto& encoder_config : config.encoders) {
        base::DictValue run_result =
            benchmark.Run(encoder_config, runtime_params, preloaded_frames);
        if (!run_result.empty()) {
          all_results.Append(std::move(run_result));
        }
      }
    }
  }

  if (!config.output_json_path.empty()) {
    base::DictValue root_output;
    root_output.Set("results", std::move(all_results));
    std::string json_output;
    if (base::JSONWriter::WriteWithOptions(
            root_output, base::JSONWriter::OPTIONS_PRETTY_PRINT,
            &json_output) &&
        base::WriteFile(config.output_json_path, json_output)) {
      std::cout << "Wrote benchmark results to " << config.output_json_path
                << std::endl;
    } else {
      LOG(ERROR) << "Failed to write JSON results to "
                 << config.output_json_path;
      return 1;
    }
  }

  return 0;
}
