// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/test/raw_video.h"
#include "media/gpu/test/video_encoder/bitstream_file_writer.h"
#include "media/gpu/test/video_encoder/bitstream_validator.h"
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"
#include "media/gpu/test/video_encoder/video_encoder.h"
#include "media/gpu/test/video_encoder/video_encoder_client.h"
#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video encoder perf tests usage message. Make sure to also update the
// documentation under docs/media/gpu/video_encoder_perf_test_usage.md when
// making changes here.
// TODO(b/211783271): Add video_encoder_perf_test_usage.md
constexpr const char* usage_msg =
    R"(usage: video_encode_accelerator_perf_tests --(speed|quality)
           [--codec=<codec>] [--svc_mode=<svc scalability mode>]
           [--content_type=(camera|display)]
           [--bitrate_mode=(cbr|vbr)] [--reverse] [--bitrate=<bitrate>]
           [-v=<level>] [--vmodule=<config>] [--output_folder]
           [--output_bitstream]
           [--gtest_help] [--help]
           [<video path>] [<video metadata path>]
)";

// Video encoder performance tests help message.
constexpr const char* help_msg =
    R"""(Run the video encode accelerator performance tests on the video
specified by <video path>. If no <video path> is given the default
"bear_320x192_40frames.yuv.webm" video will be used.

The <video metadata path> should specify the location of a json file
containing the video's metadata. By default <video path>.json will be
used.

The following arguments are supported:
   -v                   enable verbose mode, e.g. -v=2.
  --vmodule             enable verbose mode for the specified module,

  --(speed|quality)     the test cases to be run
  --codec               codec profile to encode, "h264 (baseline)",
                        "h264main, "h264high", "vp8", "vp9", "av1".
  --num_spatial_layers  the number of spatial layers of the encoded
                        bitstream. A default value is 1. Only affected
                        if --codec=vp9 currently.
  --num_temporal_layers the number of temporal layers of the encoded
                        bitstream. A default value is 1.
  --svc_mode            SVC scalability mode. Spatial SVC encoding is only
                        supported with --codec=vp9 and only runs in NV12Dmabuf
                        test cases. The valid svc mode is "L1T1", "L1T2",
                        "L1T3", "L2T1_KEY", "L2T2_KEY", "L2T3_KEY", "L3T1_KEY",
                        "L3T2_KEY", "L3T3_KEY", "S2T1", "S2T2", "S2T3", "S3T1",
                        "S3T2", "S3T3". The default value is "L1T1".
  --bitrate_mode        The rate control mode for encoding, one of "cbr"
                        (default) or "vbr".
  --reverse             the stream plays backwards if the stream reaches
                        end of stream. So the input stream to be encoded
                        is consecutive. By default this is false.
  --bitrate             bitrate (bits in second) of a produced bitstram.
                        If not specified, a proper value for the video
                        resolution is selected by the test.
  --output_folder       overwrite the output folder used to store
                        performance metrics, if not specified results
                        will be stored in the current working directory.
  --output_bitstream    save the output bitstream in either H264 AnnexB
                        format (for H264) or IVF format (for vp8 and
                        vp9) to <output_folder>/<testname>.

  --gtest_help          display the gtest help and exit.
  --help                display this help and exit.
)""";

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("bear_320x192_40frames.yuv.webm");

media::test::VideoEncoderTestEnvironment* g_env;

constexpr size_t kNumEncodeFramesForSpeedPerformance = 300;

// We use a longer event timeout because it takes much longer to encode,
// especially in 2160p, |kNumEncodeFramesForSpeedPerformance| frames in speed
// tests and decode and encode the entire videos in quality tests.
constexpr base::TimeDelta kSpeedTestEventTimeout = base::Minutes(3);
constexpr base::TimeDelta kQualityTestEventTimeout = base::Minutes(15);

// Default output folder used to store performance metrics.
constexpr const base::FilePath::CharType* kDefaultOutputFolder =
    FILE_PATH_LITERAL("perf_metrics");

// Struct storing various time-related statistics.
struct PerformanceTimeStats {
  PerformanceTimeStats() {}
  explicit PerformanceTimeStats(const std::vector<double>& times);
  double avg_ms_ = 0.0;
  double percentile_25_ms_ = 0.0;
  double percentile_50_ms_ = 0.0;
  double percentile_75_ms_ = 0.0;
};

PerformanceTimeStats::PerformanceTimeStats(const std::vector<double>& times) {
  if (times.empty())
    return;

  avg_ms_ = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
  std::vector<double> sorted_times = times;
  std::sort(sorted_times.begin(), sorted_times.end());
  percentile_25_ms_ = sorted_times[sorted_times.size() / 4];
  percentile_50_ms_ = sorted_times[sorted_times.size() / 2];
  percentile_75_ms_ = sorted_times[(sorted_times.size() * 3) / 4];
}

// TODO(dstaessens): Investigate using more appropriate metrics for encoding.
struct PerformanceMetrics {
  // Write the collected performance metrics to the console.
  void WriteToConsole() const;
  // Write the collected performance metrics to file.
  void WriteToFile() const;

  // Total measurement duration.
  base::TimeDelta total_duration_;
  // The number of bitstreams encoded.
  size_t bitstreams_encoded_ = 0;
  // The overall number of bitstreams encoded per second.
  double bitstreams_per_second_ = 0.0;
  // List of times between subsequent bitstream buffer deliveries. This is
  // important in real-time encoding scenarios, where the delivery time should
  // be less than the frame rate used.
  std::vector<double> bitstream_delivery_times_;
  // Statistics related to the time between bitstream buffer deliveries.
  PerformanceTimeStats bitstream_delivery_stats_;
  // List of times between queuing an encode operation and getting back the
  // encoded bitstream buffer.
  std::vector<double> bitstream_encode_times_;
  // Statistics related to the encode times.
  PerformanceTimeStats bitstream_encode_stats_;
};

// The performance evaluator can be plugged into the video encoder to collect
// various performance metrics.
class PerformanceEvaluator : public BitstreamProcessor {
 public:
  // Create a new performance evaluator.
  PerformanceEvaluator() {}

  void ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                        size_t frame_index) override;
  bool WaitUntilDone() override { return true; }

  // Start/Stop collecting performance metrics.
  void StartMeasuring();
  void StopMeasuring();

  // Get the collected performance metrics.
  const PerformanceMetrics& Metrics() const { return perf_metrics_; }

 private:
  // Start/end time of the measurement period.
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;

  // Time at which the previous bitstream was delivered.
  base::TimeTicks prev_bitstream_delivery_time_;

  // Collection of various performance metrics.
  PerformanceMetrics perf_metrics_;
};

void PerformanceEvaluator::ProcessBitstream(
    scoped_refptr<BitstreamRef> bitstream,
    size_t frame_index) {
  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta delivery_time = (now - prev_bitstream_delivery_time_);
  perf_metrics_.bitstream_delivery_times_.push_back(
      delivery_time.InMillisecondsF());
  prev_bitstream_delivery_time_ = now;

  // TODO(hiroh): |encode_time| on upper spatial layer in SVC encoding becomes
  // larger because the bitstram is produced after lower spatial layers are
  // produced. |encode_time| should be aggregated per spatial layer.
  base::TimeDelta encode_time =
      base::TimeTicks::Now() - bitstream->source_timestamp;
  perf_metrics_.bitstream_encode_times_.push_back(
      encode_time.InMillisecondsF());
}

void PerformanceEvaluator::StartMeasuring() {
  start_time_ = base::TimeTicks::Now();
  prev_bitstream_delivery_time_ = start_time_;
}

void PerformanceEvaluator::StopMeasuring() {
  DCHECK_EQ(perf_metrics_.bitstream_delivery_times_.size(),
            perf_metrics_.bitstream_encode_times_.size());

  end_time_ = base::TimeTicks::Now();
  perf_metrics_.total_duration_ = end_time_ - start_time_;
  perf_metrics_.bitstreams_encoded_ =
      perf_metrics_.bitstream_encode_times_.size();
  perf_metrics_.bitstreams_per_second_ =
      perf_metrics_.bitstreams_encoded_ /
      perf_metrics_.total_duration_.InSecondsF();

  // Calculate delivery and encode time metrics.
  perf_metrics_.bitstream_delivery_stats_ =
      PerformanceTimeStats(perf_metrics_.bitstream_delivery_times_);
  perf_metrics_.bitstream_encode_stats_ =
      PerformanceTimeStats(perf_metrics_.bitstream_encode_times_);
}

void PerformanceMetrics::WriteToConsole() const {
  std::cout << "Bitstreams encoded:     " << bitstreams_encoded_ << std::endl;
  std::cout << "Total duration:         " << total_duration_.InMillisecondsF()
            << "ms" << std::endl;
  std::cout << "FPS:                    " << bitstreams_per_second_
            << std::endl;
  std::cout << "Bitstream delivery time - average:       "
            << bitstream_delivery_stats_.avg_ms_ << "ms" << std::endl;
  std::cout << "Bitstream delivery time - percentile 25: "
            << bitstream_delivery_stats_.percentile_25_ms_ << "ms" << std::endl;
  std::cout << "Bitstream delivery time - percentile 50: "
            << bitstream_delivery_stats_.percentile_50_ms_ << "ms" << std::endl;
  std::cout << "Bitstream delivery time - percentile 75: "
            << bitstream_delivery_stats_.percentile_75_ms_ << "ms" << std::endl;
  std::cout << "Bitstream encode time - average:       "
            << bitstream_encode_stats_.avg_ms_ << "ms" << std::endl;
  std::cout << "Bitstream encode time - percentile 25: "
            << bitstream_encode_stats_.percentile_25_ms_ << "ms" << std::endl;
  std::cout << "Bitstream encode time - percentile 50: "
            << bitstream_encode_stats_.percentile_50_ms_ << "ms" << std::endl;
  std::cout << "Bitstream encode time - percentile 75: "
            << bitstream_encode_stats_.percentile_75_ms_ << "ms" << std::endl;
}

void PerformanceMetrics::WriteToFile() const {
  base::FilePath output_folder_path = base::FilePath(g_env->OutputFolder());
  if (!DirectoryExists(output_folder_path))
    base::CreateDirectory(output_folder_path);
  output_folder_path = base::MakeAbsoluteFilePath(output_folder_path);

  // Write performance metrics to json.
  base::Value::Dict metrics;
  metrics.Set("BitstreamsEncoded",
              base::checked_cast<int>(bitstreams_encoded_));
  metrics.Set("TotalDurationMs", total_duration_.InMillisecondsF());
  metrics.Set("FPS", bitstreams_per_second_);
  metrics.Set("BitstreamDeliveryTimeAverage",
              bitstream_delivery_stats_.avg_ms_);
  metrics.Set("BitstreamDeliveryTimePercentile25",
              bitstream_delivery_stats_.percentile_25_ms_);
  metrics.Set("BitstreamDeliveryTimePercentile50",
              bitstream_delivery_stats_.percentile_50_ms_);
  metrics.Set("BitstreamDeliveryTimePercentile75",
              bitstream_delivery_stats_.percentile_75_ms_);
  metrics.Set("BitstreamEncodeTimeAverage", bitstream_encode_stats_.avg_ms_);
  metrics.Set("BitstreamEncodeTimePercentile25",
              bitstream_encode_stats_.percentile_25_ms_);
  metrics.Set("BitstreamEncodeTimePercentile50",
              bitstream_encode_stats_.percentile_50_ms_);
  metrics.Set("BitstreamEncodeTimePercentile75",
              bitstream_encode_stats_.percentile_75_ms_);

  // Write bitstream delivery times to json.
  base::Value::List delivery_times;
  for (double bitstream_delivery_time : bitstream_delivery_times_) {
    delivery_times.Append(bitstream_delivery_time);
  }
  metrics.Set("BitstreamDeliveryTimes", std::move(delivery_times));

  // Write bitstream encodes times to json.
  base::Value::List encode_times;
  for (double bitstream_encode_time : bitstream_encode_times_) {
    encode_times.Append(bitstream_encode_time);
  }
  metrics.Set("BitstreamEncodeTimes", std::move(encode_times));

  // Write json to file.
  std::string metrics_str;
  ASSERT_TRUE(base::JSONWriter::WriteWithOptions(
      metrics, base::JSONWriter::OPTIONS_PRETTY_PRINT, &metrics_str));
  base::FilePath metrics_file_path = output_folder_path.Append(
      g_env->GetTestOutputFilePath().AddExtension(FILE_PATH_LITERAL(".json")));
  // Make sure that the directory into which json is saved is created.
  LOG_ASSERT(base::CreateDirectory(metrics_file_path.DirName()));
  base::File metrics_output_file(
      base::FilePath(metrics_file_path),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  int bytes_written = metrics_output_file.WriteAtCurrentPos(
      metrics_str.data(), metrics_str.length());
  ASSERT_EQ(bytes_written, static_cast<int>(metrics_str.length()));
  VLOG(0) << "Wrote performance metrics to: " << metrics_file_path;
}

struct BitstreamQualityMetrics {
  BitstreamQualityMetrics(
      const PSNRVideoFrameValidator* const psnr_validator,
      const SSIMVideoFrameValidator* const ssim_validator,
      const PSNRVideoFrameValidator* const bottom_row_psnr_validator,
      const LogLikelihoodRatioVideoFrameValidator* const
          log_likelihood_validator,
      const DecoderBufferValidator* const decoder_buffer_validator,
      const std::optional<size_t>& spatial_idx,
      const std::optional<size_t>& temporal_idx,
      size_t num_spatial_layers,
      SVCInterLayerPredMode inter_layer_pred_mode);

  void Output(uint32_t target_bitrate, uint32_t actual_bitrate);

  std::optional<size_t> spatial_idx;
  std::optional<size_t> temporal_idx;

 private:
  struct QualityStats {
    QualityStats() = default;
    QualityStats(const QualityStats&) = default;
    QualityStats& operator=(const QualityStats&) = default;

    double avg = 0;
    double percentile_25 = 0;
    double percentile_50 = 0;
    double percentile_75 = 0;
    std::vector<double> values_in_order;
  };

  static QualityStats ComputeQualityStats(
      const std::map<size_t, double>& values);
  static QualityStats ComputeQualityStats(const std::vector<int>& values);
  void WriteToConsole(
      const std::string& svc_text,
      const BitstreamQualityMetrics::QualityStats& psnr_stats,
      const BitstreamQualityMetrics::QualityStats& ssim_stats,
      const BitstreamQualityMetrics::QualityStats& bottom_row_psnr_stats,
      const BitstreamQualityMetrics::QualityStats& log_likelihood_stats,
      const BitstreamQualityMetrics::QualityStats& qp_stats,
      uint32_t target_bitrate,
      uint32_t actual_bitrate) const;
  void WriteToFile(
      const std::string& svc_text,
      const BitstreamQualityMetrics::QualityStats& psnr_stats,
      const BitstreamQualityMetrics::QualityStats& ssim_stats,
      const BitstreamQualityMetrics::QualityStats& bottom_row_psnr_stats,
      const BitstreamQualityMetrics::QualityStats& log_likelihood_stats,
      const BitstreamQualityMetrics::QualityStats& qp_stats,
      uint32_t target_bitrate,
      uint32_t actual_bitrate) const;

  const size_t num_spatial_layers_;
  const SVCInterLayerPredMode inter_layer_pred_mode;

  const raw_ptr<const PSNRVideoFrameValidator> psnr_validator;
  const raw_ptr<const SSIMVideoFrameValidator> ssim_validator;
  const raw_ptr<const PSNRVideoFrameValidator> bottom_row_psnr_validator;
  const raw_ptr<const LogLikelihoodRatioVideoFrameValidator>
      log_likelihood_validator;
  const raw_ptr<const DecoderBufferValidator> decoder_buffer_validator;
};

BitstreamQualityMetrics::BitstreamQualityMetrics(
    const PSNRVideoFrameValidator* const psnr_validator,
    const SSIMVideoFrameValidator* const ssim_validator,
    const PSNRVideoFrameValidator* const bottom_row_psnr_validator,
    const LogLikelihoodRatioVideoFrameValidator* const log_likelihood_validator,
    const DecoderBufferValidator* const decoder_buffer_validator,
    const std::optional<size_t>& spatial_idx,
    const std::optional<size_t>& temporal_idx,
    size_t num_spatial_layers,
    SVCInterLayerPredMode inter_layer_pred_mode)
    : spatial_idx(spatial_idx),
      temporal_idx(temporal_idx),
      num_spatial_layers_(num_spatial_layers),
      inter_layer_pred_mode(inter_layer_pred_mode),
      psnr_validator(psnr_validator),
      ssim_validator(ssim_validator),
      bottom_row_psnr_validator(bottom_row_psnr_validator),
      log_likelihood_validator(log_likelihood_validator),
      decoder_buffer_validator(decoder_buffer_validator) {}

// static
BitstreamQualityMetrics::QualityStats
BitstreamQualityMetrics::ComputeQualityStats(
    const std::map<size_t, double>& values) {
  if (values.empty())
    return QualityStats();
  std::vector<double> sorted_values;
  std::vector<std::pair<size_t, double>> index_and_value;
  sorted_values.reserve(values.size());
  index_and_value.reserve(values.size());
  for (const auto& v : values) {
    sorted_values.push_back(v.second);
    index_and_value.emplace_back(v.first, v.second);
  }
  std::sort(sorted_values.begin(), sorted_values.end());
  std::sort(index_and_value.begin(), index_and_value.end());
  QualityStats stats;
  stats.avg = std::accumulate(sorted_values.begin(), sorted_values.end(), 0.0) /
              sorted_values.size();
  stats.percentile_25 = sorted_values[sorted_values.size() / 4];
  stats.percentile_50 = sorted_values[sorted_values.size() / 2];
  stats.percentile_75 = sorted_values[(sorted_values.size() * 3) / 4];
  stats.values_in_order.resize(index_and_value.size());
  for (size_t i = 0; i < index_and_value.size(); ++i)
    stats.values_in_order[i] = index_and_value[i].second;
  return stats;
}

// static
BitstreamQualityMetrics::QualityStats
BitstreamQualityMetrics::ComputeQualityStats(const std::vector<int>& values) {
  if (values.empty()) {
    return QualityStats();
  }

  std::vector<int> sorted_values = values;
  QualityStats stats;
  std::sort(sorted_values.begin(), sorted_values.end());
  stats.avg = std::accumulate(sorted_values.begin(), sorted_values.end(), 0.0) /
              sorted_values.size();
  stats.percentile_25 = sorted_values[sorted_values.size() / 4];
  stats.percentile_50 = sorted_values[sorted_values.size() / 2];
  stats.percentile_75 = sorted_values[(sorted_values.size() * 3) / 4];
  stats.values_in_order = std::vector<double>(values.begin(), values.end());
  return stats;
}

void BitstreamQualityMetrics::Output(uint32_t target_bitrate,
                                     uint32_t actual_bitrate) {
  std::string svc_text;
  if (spatial_idx) {
    svc_text += (inter_layer_pred_mode == SVCInterLayerPredMode::kOff &&
                         num_spatial_layers_ > 1
                     ? "S"
                     : "L") +
                base::NumberToString(*spatial_idx + 1);
  }
  if (temporal_idx)
    svc_text += "T" + base::NumberToString(*temporal_idx + 1);

  auto psnr_stats = ComputeQualityStats(psnr_validator->GetPSNRValues());
  auto ssim_stats = ComputeQualityStats(ssim_validator->GetSSIMValues());
  auto bottom_row_psnr_stats =
      ComputeQualityStats(bottom_row_psnr_validator->GetPSNRValues());
  auto log_likelihood_stats = ComputeQualityStats(
      log_likelihood_validator->get_log_likelihood_ratio_values());
  auto qp_stats = ComputeQualityStats(decoder_buffer_validator->GetQPValues(
      spatial_idx.value_or(0), temporal_idx.value_or(0)));
  WriteToConsole(svc_text, psnr_stats, ssim_stats, bottom_row_psnr_stats,
                 log_likelihood_stats, qp_stats, target_bitrate,
                 actual_bitrate);
  WriteToFile(svc_text, psnr_stats, ssim_stats, bottom_row_psnr_stats,
              log_likelihood_stats, qp_stats, target_bitrate, actual_bitrate);
}

void BitstreamQualityMetrics::WriteToConsole(
    const std::string& svc_text,
    const BitstreamQualityMetrics::QualityStats& psnr_stats,
    const BitstreamQualityMetrics::QualityStats& ssim_stats,
    const BitstreamQualityMetrics::QualityStats& bottom_row_psnr_stats,
    const BitstreamQualityMetrics::QualityStats& log_likelihood_stats,
    const BitstreamQualityMetrics::QualityStats& qp_stats,
    uint32_t target_bitrate,
    uint32_t actual_bitrate) const {
  const auto default_ssize = std::cout.precision();
  std::cout << "[ Result " << svc_text << "]" << std::endl;
  std::cout << "Bitrate: " << actual_bitrate << " (target:  " << target_bitrate
            << ")" << std::endl;
  std::cout << "Bitrate deviation: " << std::fixed << std::setprecision(2)
            << (actual_bitrate * 100.0 / target_bitrate) - 100.0 << " %"
            << std::endl;

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "SSIM - average:       " << ssim_stats.avg << std::endl;
  std::cout << "SSIM - percentile 25: " << ssim_stats.percentile_25
            << std::endl;
  std::cout << "SSIM - percentile 50: " << ssim_stats.percentile_50
            << std::endl;
  std::cout << "SSIM - percentile 75: " << ssim_stats.percentile_75
            << std::endl;
  std::cout << "PSNR - average:       " << psnr_stats.avg << std::endl;
  std::cout << "PSNR - percentile 25: " << psnr_stats.percentile_25
            << std::endl;
  std::cout << "PSNR - percentile 50: " << psnr_stats.percentile_50
            << std::endl;
  std::cout << "PSNR - percentile 75: " << psnr_stats.percentile_75
            << std::endl;
  std::cout << "Bottom row PSNR - average:       " << bottom_row_psnr_stats.avg
            << std::endl;
  std::cout << "Bottom row PSNR - percentile 25: "
            << bottom_row_psnr_stats.percentile_25 << std::endl;
  std::cout << "Bottom row PSNR - percentile 50: "
            << bottom_row_psnr_stats.percentile_50 << std::endl;
  std::cout << "Bottom row PSNR - percentile 75: "
            << bottom_row_psnr_stats.percentile_75 << std::endl;
  std::cout << "Log likelihood ratio - average:       "
            << log_likelihood_stats.avg << std::endl;
  std::cout << "Log likelihood ratio - percentile 25: "
            << log_likelihood_stats.percentile_25 << std::endl;
  std::cout << "Log likelihood ratio - percentile 50: "
            << log_likelihood_stats.percentile_50 << std::endl;
  std::cout << "Log likelihood ratio - percentile 75: "
            << log_likelihood_stats.percentile_75 << std::endl;
  std::cout << "QP - average:       " << qp_stats.avg << std::endl;
  std::cout << "QP - percentile 25: " << qp_stats.percentile_25 << std::endl;
  std::cout << "QP - percentile 50: " << qp_stats.percentile_50 << std::endl;
  std::cout << "QP - percentile 75: " << qp_stats.percentile_75 << std::endl;
  std::cout.precision(default_ssize);
}

void BitstreamQualityMetrics::WriteToFile(
    const std::string& svc_text,
    const BitstreamQualityMetrics::QualityStats& psnr_stats,
    const BitstreamQualityMetrics::QualityStats& ssim_stats,
    const BitstreamQualityMetrics::QualityStats& bottom_row_psnr_stats,
    const BitstreamQualityMetrics::QualityStats& log_likelihood_stats,
    const BitstreamQualityMetrics::QualityStats& qp_stats,
    uint32_t target_bitrate,
    uint32_t actual_bitrate) const {
  base::FilePath output_folder_path = base::FilePath(g_env->OutputFolder());
  if (!DirectoryExists(output_folder_path))
    base::CreateDirectory(output_folder_path);
  output_folder_path = base::MakeAbsoluteFilePath(output_folder_path);
  // Write quality metrics to json.
  base::Value::Dict metrics;
  if (!svc_text.empty())
    metrics.Set("SVC", svc_text);
  metrics.Set("Bitrate", base::checked_cast<int>(actual_bitrate));
  metrics.Set("BitrateDeviation",
              (actual_bitrate * 100.0 / target_bitrate) - 100.0);
  metrics.Set("SSIMAverage", ssim_stats.avg);
  metrics.Set("PSNRAverage", psnr_stats.avg);
  metrics.Set("BottomRowPSNRAverage", bottom_row_psnr_stats.avg);
  metrics.Set("LogLikelihoodRatioAverage", log_likelihood_stats.avg);
  // Write ssim values bitstream delivery times to json.
  base::Value::List ssim_values;
  for (double value : ssim_stats.values_in_order)
    ssim_values.Append(value);
  metrics.Set("SSIMValues", std::move(ssim_values));

  // Write psnr values to json.
  base::Value::List psnr_values;
  for (double value : psnr_stats.values_in_order)
    psnr_values.Append(value);
  metrics.Set("PSNRValues", std::move(psnr_values));

  // Write bottom row psnr values to json.
  base::Value::List bottom_row_psnr_values;
  for (double value : bottom_row_psnr_stats.values_in_order)
    bottom_row_psnr_values.Append(value);
  metrics.Set("BottomRowPSNRValues", std::move(bottom_row_psnr_values));

  // Write log likelihood ratio values to json.
  base::Value::List log_likelihood_values;
  for (double value : log_likelihood_stats.values_in_order) {
    log_likelihood_values.Append(value);
  }
  metrics.Set("LogLikelihoodRatioValues", std::move(log_likelihood_values));

  // Write QP values to json.
  base::Value::List qp_values;
  for (double value : qp_stats.values_in_order) {
    qp_values.Append(value);
  }
  metrics.Set("QPValues", std::move(qp_values));

  // Write json to file.
  std::string metrics_str;
  ASSERT_TRUE(base::JSONWriter::WriteWithOptions(
      metrics, base::JSONWriter::OPTIONS_PRETTY_PRINT, &metrics_str));
  constexpr const base::FilePath::CharType* kMetrixFileSuffix =
      FILE_PATH_LITERAL(".json");
  const std::string svc_text_ext = svc_text.empty() ? "" : "." + svc_text;
  base::FilePath metrics_file_path =
      output_folder_path.Append(g_env->GetTestOutputFilePath()
                                    .AddExtensionASCII(svc_text_ext)
                                    .AddExtension(kMetrixFileSuffix));
  // Make sure that the directory into which json is saved is created.
  LOG_ASSERT(base::CreateDirectory(metrics_file_path.DirName()));
  base::File metrics_output_file(
      base::FilePath(metrics_file_path),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  int bytes_written = metrics_output_file.WriteAtCurrentPos(
      metrics_str.data(), metrics_str.length());
  ASSERT_EQ(bytes_written, static_cast<int>(metrics_str.length()));
  VLOG(0) << "Wrote performance metrics to: " << metrics_file_path;
}

// Video encode test class. Performs setup and teardown for each single test.
// It measures the performance in encoding NV12 GpuMemoryBuffer based
// VideoFrame.
class VideoEncoderTest : public ::testing::Test {
 public:
  // Creates VideoEncoder for encoding NV12 GpuMemoryBuffer based VideoFrames.
  // The input VideoFrames are provided every 1 / |encoder_rate| seconds if it
  // is specified. Or they are provided as soon as the previous input VideoFrame
  // is consumed by VideoEncoder. |measure_quality| measures SSIM and PSNR
  // values of encoded bitstream comparing the original input VideoFrames.
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      std::optional<uint32_t> encode_rate = std::nullopt,
      bool measure_quality = false,
      size_t num_encode_frames = kNumEncodeFramesForSpeedPerformance) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    RawVideo* video = g_env->GenerateNV12Video();
#else
    // TODO(b/211783271): Add support for I420 SHM input.
    RawVideo* video = g_env->Video();
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    VideoCodecProfile profile = g_env->Profile();
    const media::VideoBitrateAllocation& bitrate = g_env->BitrateAllocation();
    const std::vector<VideoEncodeAccelerator::Config::SpatialLayer>&
        spatial_layers = g_env->SpatialLayers();
    SVCInterLayerPredMode inter_layer_pred_mode = g_env->InterLayerPredMode();
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;
    if (measure_quality) {
      bitstream_processors = CreateBitstreamProcessorsForQualityPerformance(
          video, profile, num_encode_frames, spatial_layers,
          inter_layer_pred_mode);
    } else {
      auto performance_evaluator = std::make_unique<PerformanceEvaluator>();
      performance_evaluator_ = performance_evaluator.get();
      bitstream_processors.push_back(std::move(performance_evaluator));
    }
    LOG_ASSERT(!bitstream_processors.empty())
        << "Failed to create bitstream processors";

    VideoEncoderClientConfig config(
        video, profile, spatial_layers, g_env->InterLayerPredMode(),
        g_env->ContentType(), bitrate, g_env->Reverse());
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    config.input_storage_type =
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
#else
    // TODO(https://crbugs.com/350994517, b/211783271): Enable GMB for
    // Windows/Linux.
    config.input_storage_type =
        VideoEncodeAccelerator::Config::StorageType::kShmem;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    config.num_frames_to_encode = num_encode_frames;
    if (encode_rate) {
      config.encode_interval = base::Seconds(1u) / encode_rate.value();
    }

    auto video_encoder =
        VideoEncoder::Create(config, std::move(bitstream_processors));
    LOG_ASSERT(video_encoder);
    LOG_ASSERT(video_encoder->Initialize(video));

    return video_encoder;
  }

 protected:
  raw_ptr<PerformanceEvaluator> performance_evaluator_;
  raw_ptr<DecoderBufferValidator> decoder_buffer_validator_;
  std::vector<BitstreamQualityMetrics> quality_metrics_;

 private:
  std::unique_ptr<BitstreamValidator> CreateBitstreamValidator(
      const VideoCodecProfile profile,
      const gfx::Rect& visible_rect,
      size_t num_encode_frames,
      const std::optional<size_t>& spatial_layer_index_to_decode,
      const std::optional<size_t>& temporal_layer_index_to_decode,
      const std::vector<gfx::Size>& spatial_layer_resolutions,
      DecoderBufferValidator* const decoder_buffer_validator,
      SVCInterLayerPredMode inter_layer_pred_mode) {
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors;
    VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
        base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                            base::Unretained(this), visible_rect);
    auto ssim_validator = SSIMVideoFrameValidator::Create(
        get_model_frame_cb, /*corrupt_frame_processor=*/nullptr,
        VideoFrameValidator::ValidationMode::kAverage,
        /*tolerance=*/0.0);
    LOG_ASSERT(ssim_validator);
    auto psnr_validator = PSNRVideoFrameValidator::Create(
        get_model_frame_cb, /*corrupt_frame_processor=*/nullptr,
        VideoFrameValidator::ValidationMode::kAverage,
        /*tolerance=*/0.0);
    LOG_ASSERT(psnr_validator);
    auto bottom_row_psnr_validator = PSNRVideoFrameValidator::Create(
        get_model_frame_cb,
        /*corrupt_frame_processor=*/nullptr,
        VideoFrameValidator::ValidationMode::kAverage,
        /*tolerance=*/0.0,
        base::BindRepeating(&BottomRowCrop, kDefaultBottomRowCropHeight));
    LOG_ASSERT(bottom_row_psnr_validator);
    auto log_likelihood_validator =
        LogLikelihoodRatioVideoFrameValidator::Create(
            get_model_frame_cb,
            /*corrupt_frame_processor=*/nullptr,
            VideoFrameValidator::ValidationMode::kAverage,
            /*tolerance=*/100.0);
    LOG_ASSERT(log_likelihood_validator);
    quality_metrics_.push_back(BitstreamQualityMetrics(
        psnr_validator.get(), ssim_validator.get(),
        bottom_row_psnr_validator.get(), log_likelihood_validator.get(),
        decoder_buffer_validator, spatial_layer_index_to_decode,
        temporal_layer_index_to_decode, spatial_layer_resolutions.size(),
        inter_layer_pred_mode));
    video_frame_processors.push_back(std::move(ssim_validator));
    video_frame_processors.push_back(std::move(psnr_validator));
    video_frame_processors.push_back(std::move(bottom_row_psnr_validator));
    video_frame_processors.push_back(std::move(log_likelihood_validator));

    VideoDecoderConfig decoder_config(
        VideoCodecProfileToVideoCodec(profile), profile,
        VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
        kNoTransformation, visible_rect.size(), visible_rect,
        visible_rect.size(), EmptyExtraData(), EncryptionScheme::kUnencrypted);

    return BitstreamValidator::Create(
        decoder_config, num_encode_frames - 1,
        std::move(video_frame_processors), spatial_layer_index_to_decode,
        temporal_layer_index_to_decode, spatial_layer_resolutions);
  }

  // Create bitstream processors for quality performance tests.
  std::vector<std::unique_ptr<BitstreamProcessor>>
  CreateBitstreamProcessorsForQualityPerformance(
      RawVideo* video,
      VideoCodecProfile profile,
      size_t num_encode_frames,
      const std::vector<VideoEncodeAccelerator::Config::SpatialLayer>&
          spatial_layers,
      SVCInterLayerPredMode inter_layer_pred_mode) {
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;

    raw_data_helper_ = std::make_unique<RawDataHelper>(video, g_env->Reverse());

    auto decoder_buffer_validator = DecoderBufferValidator::Create(
        profile, gfx::Rect(video->Resolution()),
        spatial_layers.empty() ? 1u : spatial_layers.size(),
        spatial_layers.empty() ? 1u : spatial_layers[0].num_of_temporal_layers,
        inter_layer_pred_mode);
    decoder_buffer_validator_ = decoder_buffer_validator.get();
    bitstream_processors.push_back(std::move(decoder_buffer_validator));
    if (spatial_layers.empty()) {
      // Simple stream encoding.
      bitstream_processors.push_back(CreateBitstreamValidator(
          profile, gfx::Rect(video->Resolution()), num_encode_frames,
          /*spatial_layer_index_to_decode=*/std::nullopt,
          /*temporal_layer_index_to_decode=*/std::nullopt,
          /*spatial_layer_resolutions=*/{}, decoder_buffer_validator_,
          SVCInterLayerPredMode::kOff));
      if (g_env->SaveOutputBitstream()) {
        bitstream_processors.emplace_back(BitstreamFileWriter::Create(
            g_env->OutputFilePath(VideoCodecProfileToVideoCodec(profile)),
            VideoCodecProfileToVideoCodec(profile), video->Resolution(),
            video->FrameRate(), video->NumFrames()));
      }
    } else {
      // Temporal/Spatial layer encoding.
      std::vector<gfx::Size> spatial_layer_resolutions;
      for (const auto& sl : spatial_layers)
        spatial_layer_resolutions.emplace_back(sl.width, sl.height);

      for (size_t sid = 0; sid < spatial_layers.size(); ++sid) {
        for (size_t tid = 0; tid < spatial_layers[sid].num_of_temporal_layers;
             ++tid) {
          bitstream_processors.push_back(CreateBitstreamValidator(
              profile, gfx::Rect(spatial_layer_resolutions[sid]),
              num_encode_frames, sid, tid, spatial_layer_resolutions,
              decoder_buffer_validator_, inter_layer_pred_mode));
          if (g_env->SaveOutputBitstream()) {
            bitstream_processors.emplace_back(BitstreamFileWriter::Create(
                g_env->OutputFilePath(VideoCodecProfileToVideoCodec(profile),
                                      true, sid, tid),
                VideoCodecProfileToVideoCodec(profile),
                spatial_layer_resolutions[sid], video->FrameRate(),
                video->NumFrames(), sid, tid, spatial_layer_resolutions));
          }
        }
      }
    }
    LOG_ASSERT(!base::Contains(bitstream_processors, nullptr));
    return bitstream_processors;
  }

  scoped_refptr<const VideoFrame> GetModelFrame(const gfx::Rect& visible_rect,
                                                size_t frame_index) {
    LOG_ASSERT(raw_data_helper_);
    auto frame = raw_data_helper_->GetFrame(frame_index);
    if (!frame)
      return nullptr;
    if (visible_rect.size() == frame->visible_rect().size())
      return frame;
    return ScaleVideoFrame(frame.get(), visible_rect.size());
  }

  std::unique_ptr<RawDataHelper> raw_data_helper_;
};

}  // namespace

// Encode |kNumEncodeFramesForSpeedPerformance| frames while measuring
// uncapped performance. This test will encode a video as fast as
// possible, and gives an idea about the maximum output of the
// encoder.
TEST_F(VideoEncoderTest, MeasureUncappedPerformance) {
  if (g_env->RunTestType() !=
      media::test::VideoEncoderTestEnvironment::TestType::kSpeedPerformance) {
    GTEST_SKIP()
        << "Skip because this test case is to measure speed performance";
  }
  auto encoder = CreateVideoEncoder();
  encoder->SetEventWaitTimeout(kSpeedTestEventTimeout);

  performance_evaluator_->StartMeasuring();
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  performance_evaluator_->StopMeasuring();

  auto metrics = performance_evaluator_->Metrics();
  metrics.WriteToConsole();
  metrics.WriteToFile();

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(),
            kNumEncodeFramesForSpeedPerformance);
}

// TODO(b/211783279) The |performance_evaluator_| only keeps track of the last
// created encoder. We should instead keep track of multiple evaluators, and
// then decide how to aggregate/report those metrics.
TEST_F(VideoEncoderTest,
       MeasureUncappedPerformance_MultipleConcurrentEncoders) {
  if (g_env->RunTestType() !=
      media::test::VideoEncoderTestEnvironment::TestType::kSpeedPerformance) {
    GTEST_SKIP()
        << "Skip because this test case is to measure speed performance";
  }
  // Run two encoders for larger resolutions to avoid creating shared memory
  // buffers during the test on lower end devices.
  constexpr gfx::Size k1080p(1920, 1080);
  const size_t kMinSupportedConcurrentEncoders =
      g_env->Video()->Resolution().GetArea() >= k1080p.GetArea() ? 2 : 3;

  std::vector<std::unique_ptr<VideoEncoder>> encoders(
      kMinSupportedConcurrentEncoders);
  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i) {
    encoders[i] = CreateVideoEncoder();
    encoders[i]->SetEventWaitTimeout(kSpeedTestEventTimeout);
  }

  performance_evaluator_->StartMeasuring();

  for (auto&& encoder : encoders) {
    encoder->Encode();
  }

  for (auto&& encoder : encoders) {
    EXPECT_TRUE(encoder->WaitForFlushDone());
    EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
    EXPECT_EQ(encoder->GetFrameReleasedCount(),
              kNumEncodeFramesForSpeedPerformance);
  }

  performance_evaluator_->StopMeasuring();
  auto metrics = performance_evaluator_->Metrics();
  metrics.WriteToConsole();
  metrics.WriteToFile();
}

// Encode |kNumEncodeFramesForSpeedPerformance| frames while measuring
// capped performance. This test will encode a video at a fixed ratio,
// 30fps. This test can be used to measure the cpu metrics during
// encoding.
TEST_F(VideoEncoderTest, DISABLED_MeasureCappedPerformance) {
  if (g_env->RunTestType() !=
      media::test::VideoEncoderTestEnvironment::TestType::kSpeedPerformance) {
    GTEST_SKIP()
        << "Skip because this test case is to measure speed performance";
  }
  const uint32_t kEncodeRate = 30;
  auto encoder = CreateVideoEncoder(/*encode_rate=*/kEncodeRate);
  encoder->SetEventWaitTimeout(kSpeedTestEventTimeout);

  performance_evaluator_->StartMeasuring();
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  performance_evaluator_->StopMeasuring();

  auto metrics = performance_evaluator_->Metrics();
  metrics.WriteToConsole();
  metrics.WriteToFile();

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(),
            kNumEncodeFramesForSpeedPerformance);
}

TEST_F(VideoEncoderTest, MeasureProducedBitstreamQuality) {
  if (g_env->RunTestType() !=
      media::test::VideoEncoderTestEnvironment::TestType::kQualityPerformance) {
    GTEST_SKIP()
        << "Skip because this test case is to measure quality performance";
  }
  const size_t num_frames = g_env->Video()->NumFrames();
  auto encoder = CreateVideoEncoder(/*encode_rate=*/std::nullopt,
                                    /*measure_quality=*/true,
                                    /*num_encode_frames=*/num_frames);
  encoder->SetEventWaitTimeout(kQualityTestEventTimeout);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), num_frames);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());

  const VideoEncoderStats stats = encoder->GetStats();
  for (auto& metrics : quality_metrics_) {
    std::optional<size_t> spatial_idx = metrics.spatial_idx;
    std::optional<size_t> temporal_idx = metrics.temporal_idx;
    uint32_t target_bitrate = 0;
    uint32_t actual_bitrate = 0;
    if (!spatial_idx && !temporal_idx) {
      target_bitrate = g_env->BitrateAllocation().GetSumBps();
      actual_bitrate = stats.Bitrate();
    } else {
      CHECK(spatial_idx && temporal_idx);
      // Target and actual bitrates in temporal layer encoding are the sum of
      // bitrates of the temporal layers in the spatial layer.
      for (size_t tid = 0; tid <= *temporal_idx; ++tid) {
        target_bitrate +=
            g_env->BitrateAllocation().GetBitrateBps(*spatial_idx, tid);
        actual_bitrate += stats.LayerBitrate(*spatial_idx, tid);
      }
    }

    metrics.Output(target_bitrate, actual_bitrate);
  }
}
}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::RawVideo::SetTestDataPath(media::GetTestDataPath());

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::test::usage_msg << "\n" << media::test::help_msg;
    return 0;
  }

  // Check if a video was specified on the command line.
  base::CommandLine::StringVector args = cmd_line->GetArgs();
  media::test::VideoEncoderTestEnvironment::TestType test_type =
      media::test::VideoEncoderTestEnvironment::TestType::kValidation;
  base::FilePath video_path =
      (args.size() >= 1) ? base::FilePath(args[0])
                         : base::FilePath(media::test::kDefaultTestVideoPath);
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();
  std::string codec = "h264";
  media::VideoEncodeAccelerator::Config::ContentType content_type =
      media::VideoEncodeAccelerator::Config::ContentType::kCamera;
  media::Bitrate::Mode bitrate_mode = media::Bitrate::Mode::kConstant;
  bool reverse = false;
  bool output_bitstream = false;
  std::optional<uint32_t> encode_bitrate;
  std::vector<base::test::FeatureRef> disabled_features;
  std::string svc_mode = "L1T1";

  // Parse command line arguments.
  base::FilePath::StringType output_folder = media::test::kDefaultOutputFolder;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "num_temporal_layers" ||
        it->first == "num_spatial_layers") {
      std::cout << "--num_temporal_layers and --num_spatial_layers have been "
                << "removed. Please use --svc_mode";
      return EXIT_FAILURE;
    }

    if (it->first == "output_folder") {
      output_folder = it->second;
    } else if (it->first == "output_bitstream") {
      output_bitstream = true;
    } else if (it->first == "codec") {
      codec = cmd_line->GetSwitchValueASCII("codec");
    } else if (it->first == "svc_mode") {
      svc_mode = cmd_line->GetSwitchValueASCII("svc_mode");
    } else if (it->first == "content_type") {
      auto content_type_str = cmd_line->GetSwitchValueASCII("content_type");
      if (content_type_str == "display") {
        content_type =
            media::VideoEncodeAccelerator::Config::ContentType::kDisplay;
      } else if (content_type_str != "camera") {
        std::cout << "unknown content type \"" << it->second
                  << "\", possible values are \"camera|display\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "bitrate_mode") {
      auto brc_mode_str = cmd_line->GetSwitchValueASCII("bitrate_mode");
      if (brc_mode_str == "vbr") {
        bitrate_mode = media::Bitrate::Mode::kVariable;
      } else if (brc_mode_str != "cbr") {
        std::cout << "unknown bitrate mode \"" << it->second
                  << "\", possible values are \"cbr|vbr\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "reverse") {
      reverse = true;
    } else if (it->first == "bitrate") {
      unsigned value;
      if (!base::StringToUint(it->second, &value)) {
        std::cout << "invalid bitrate " << it->second << "\n"
                  << media::test::usage_msg;
        return EXIT_FAILURE;
      }
      encode_bitrate = base::checked_cast<uint32_t>(value);
    } else if (it->first == "speed") {
      test_type =
          media::test::VideoEncoderTestEnvironment::TestType::kSpeedPerformance;
    } else if (it->first == "quality") {
      test_type = media::test::VideoEncoderTestEnvironment::TestType::
          kQualityPerformance;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  disabled_features.push_back(media::kGlobalVaapiLock);

  if (test_type ==
      media::test::VideoEncoderTestEnvironment::TestType::kValidation) {
    std::cout << "--speed or --quality must be specified\n"
              << media::test::usage_msg;
    return EXIT_FAILURE;
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoEncoderTestEnvironment* test_environment =
      media::test::VideoEncoderTestEnvironment::Create(
          test_type, video_path, video_metadata_path,
          base::FilePath(output_folder), codec, svc_mode, content_type,
          output_bitstream, encode_bitrate, bitrate_mode, reverse,
          media::test::FrameOutputConfig(),
          /*enabled_features=*/{}, disabled_features);
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
