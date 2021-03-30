// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <numeric>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_encoder/bitstream_validator.h"
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
// TODO(dstaessens): Add video_encoder_perf_test_usage.md
constexpr const char* usage_msg =
    "usage: video_encode_accelerator_perf_tests\n"
    "           [--codec=<codec>]\n"
    "           [-v=<level>] [--vmodule=<config>] [--output_folder]\n"
    "           [--gtest_help] [--help]\n"
    "           [<video path>] [<video metadata path>]\n";

// Video encoder performance tests help message.
constexpr const char* help_msg =
    "Run the video encode accelerator performance tests on the video\n"
    "specified by <video path>. If no <video path> is given the default\n"
    "\"bear_320x192_40frames.yuv.webm\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata. By default <video path>.json will be\n"
    "used.\n"
    "\nThe following arguments are supported:\n"
    "  --codec              codec profile to encode, \"h264 (baseline)\",\n"
    "                       \"h264main, \"h264high\", \"vp8\" and \"vp9\"\n"
    "   -v                  enable verbose mode, e.g. -v=2.\n"
    "  --vmodule            enable verbose mode for the specified module,\n"
    "  --output_folder      overwrite the output folder used to store\n"
    "                       performance metrics, if not specified results\n"
    "                       will be stored in the current working directory.\n"
    "  --gtest_help         display the gtest help and exit.\n"
    "  --help               display this help and exit.\n";

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("bear_320x192_40frames.yuv.webm");

media::test::VideoEncoderTestEnvironment* g_env;

constexpr size_t kNumFramesToEncodeForPerformance = 300;

// The event timeout used in perf tests because encoding 2160p
// |kNumFramesToEncodeForPerformance| frames take much time.
constexpr base::TimeDelta kPerfEventTimeout = base::TimeDelta::FromSeconds(180);

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

  base::TimeDelta encode_time =
      now.since_origin() - bitstream->metadata.timestamp;
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
  base::Value metrics(base::Value::Type::DICTIONARY);
  metrics.SetKey("BitstreamsEncoded",
                 base::Value(base::checked_cast<int>(bitstreams_encoded_)));
  metrics.SetKey("TotalDurationMs",
                 base::Value(total_duration_.InMillisecondsF()));
  metrics.SetKey("FPS", base::Value(bitstreams_per_second_));
  metrics.SetKey("BitstreamDeliveryTimeAverage",
                 base::Value(bitstream_delivery_stats_.avg_ms_));
  metrics.SetKey("BitstreamDeliveryTimePercentile25",
                 base::Value(bitstream_delivery_stats_.percentile_25_ms_));
  metrics.SetKey("BitstreamDeliveryTimePercentile50",
                 base::Value(bitstream_delivery_stats_.percentile_50_ms_));
  metrics.SetKey("BitstreamDeliveryTimePercentile75",
                 base::Value(bitstream_delivery_stats_.percentile_75_ms_));
  metrics.SetKey("BitstreamEncodeTimeAverage",
                 base::Value(bitstream_encode_stats_.avg_ms_));
  metrics.SetKey("BitstreamEncodeTimePercentile25",
                 base::Value(bitstream_encode_stats_.percentile_25_ms_));
  metrics.SetKey("BitstreamEncodeTimePercentile50",
                 base::Value(bitstream_encode_stats_.percentile_50_ms_));
  metrics.SetKey("BitstreamEncodeTimePercentile75",
                 base::Value(bitstream_encode_stats_.percentile_75_ms_));

  // Write bitstream delivery times to json.
  base::Value delivery_times(base::Value::Type::LIST);
  for (double bitstream_delivery_time : bitstream_delivery_times_) {
    delivery_times.Append(bitstream_delivery_time);
  }
  metrics.SetKey("BitstreamDeliveryTimes", std::move(delivery_times));

  // Write bitstream encodes times to json.
  base::Value encode_times(base::Value::Type::LIST);
  for (double bitstream_encode_time : bitstream_encode_times_) {
    encode_times.Append(bitstream_encode_time);
  }
  metrics.SetKey("BitstreamEncodeTimes", std::move(encode_times));

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
  BitstreamQualityMetrics(const PSNRVideoFrameValidator& psnr_validator,
                          const SSIMVideoFrameValidator& ssim_validator);
  void WriteToConsole() const;
  void WriteToFile() const;

 private:
  struct QualityStats {
    double avg = 0;
    double percentile_25 = 0;
    double percentile_50 = 0;
    double percentile_75 = 0;
    std::vector<double> values_in_order;
  };

  static QualityStats ComputeQualityStats(
      const std::map<size_t, double>& values);

  const QualityStats psnr_stats;
  const QualityStats ssim_stats;
};

BitstreamQualityMetrics::BitstreamQualityMetrics(
    const PSNRVideoFrameValidator& psnr_validator,
    const SSIMVideoFrameValidator& ssim_validator)
    : psnr_stats(ComputeQualityStats(psnr_validator.GetPSNRValues())),
      ssim_stats(ComputeQualityStats(ssim_validator.GetSSIMValues())) {}

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

void BitstreamQualityMetrics::WriteToConsole() const {
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
}

void BitstreamQualityMetrics::WriteToFile() const {
  base::FilePath output_folder_path = base::FilePath(g_env->OutputFolder());
  if (!DirectoryExists(output_folder_path))
    base::CreateDirectory(output_folder_path);
  output_folder_path = base::MakeAbsoluteFilePath(output_folder_path);
  // Write quality metrics to json.
  base::Value metrics(base::Value::Type::DICTIONARY);
  metrics.SetKey("SSIMAverage", base::Value(ssim_stats.avg));
  metrics.SetKey("SSIMPercentile25", base::Value(ssim_stats.percentile_25));
  metrics.SetKey("SSIMPercentile50", base::Value(ssim_stats.percentile_50));
  metrics.SetKey("SSIMPercentile75", base::Value(psnr_stats.percentile_75));
  metrics.SetKey("PSNRAverage", base::Value(psnr_stats.avg));
  metrics.SetKey("PSNRPercentile25", base::Value(psnr_stats.percentile_25));
  metrics.SetKey("PSNRPercentile50", base::Value(psnr_stats.percentile_50));
  metrics.SetKey("PSNRPercentile75", base::Value(psnr_stats.percentile_75));
  // Write ssim values bitstream delivery times to json.
  base::Value ssim_values(base::Value::Type::LIST);
  for (double value : ssim_stats.values_in_order)
    ssim_values.Append(value);
  metrics.SetKey("SSIMValues", std::move(ssim_values));

  // Write psnr values to json.
  base::Value psnr_values(base::Value::Type::LIST);
  for (double value : psnr_stats.values_in_order)
    psnr_values.Append(value);
  metrics.SetKey("PSNRValues", std::move(psnr_values));

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

// Video encode test class. Performs setup and teardown for each single test.
class VideoEncoderTest : public ::testing::Test {
 public:
  // Create a new video encoder instance.
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(Video* video,
                                                   VideoCodecProfile profile,
                                                   uint32_t bitrate,
                                                   uint32_t encoder_rate = 0) {
    auto performance_evaluator = std::make_unique<PerformanceEvaluator>();
    performance_evaluator_ = performance_evaluator.get();
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;
    bitstream_processors.push_back(std::move(performance_evaluator));
    return CreateVideoEncoderWithProcessors(
        video, profile, bitrate, encoder_rate, std::move(bitstream_processors));
  }

  // Create a new video encoder instance for quality performance tests.
  std::unique_ptr<VideoEncoder> CreateVideoEncoderForQualityPerformance(
      Video* video,
      VideoCodecProfile profile,
      uint32_t bitrate) {
    raw_data_helper_ = RawDataHelper::Create(video);
    if (!raw_data_helper_) {
      LOG(ERROR) << "Failed to create raw data helper";
      return nullptr;
    }

    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors;
    VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
        base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                            base::Unretained(this));
    auto ssim_validator = SSIMVideoFrameValidator::Create(
        get_model_frame_cb, /*corrupt_frame_processor=*/nullptr,
        VideoFrameValidator::ValidationMode::kAverage,
        /*tolerance=*/0.0);
    LOG_ASSERT(ssim_validator);
    ssim_validator_ = ssim_validator.get();
    video_frame_processors.push_back(std::move(ssim_validator));
    auto psnr_validator = PSNRVideoFrameValidator::Create(
        get_model_frame_cb, /*corrupt_frame_processor=*/nullptr,
        VideoFrameValidator::ValidationMode::kAverage,
        /*tolerance=*/0.0);
    LOG_ASSERT(psnr_validator);
    psnr_validator_ = psnr_validator.get();
    video_frame_processors.push_back(std::move(psnr_validator));

    const gfx::Rect visible_rect(video->Resolution());
    VideoDecoderConfig decoder_config(
        VideoCodecProfileToVideoCodec(profile), profile,
        VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
        kNoTransformation, visible_rect.size(), visible_rect,
        visible_rect.size(), EmptyExtraData(), EncryptionScheme::kUnencrypted);

    auto bitstream_validator = BitstreamValidator::Create(
        decoder_config, kNumFramesToEncodeForPerformance - 1,
        std::move(video_frame_processors));
    LOG_ASSERT(bitstream_validator);
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;
    bitstream_processors.push_back(std::move(bitstream_validator));
    return CreateVideoEncoderWithProcessors(video, profile, bitrate,
                                            /*encoder_rate=*/0,
                                            std::move(bitstream_processors));
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoderWithProcessors(
      Video* video,
      VideoCodecProfile profile,
      uint32_t bitrate,
      uint32_t encoder_rate,
      std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors) {
    LOG_ASSERT(video);
    constexpr size_t kNumTemporalLayers = 1u;
    VideoEncoderClientConfig config(video, profile, kNumTemporalLayers,
                                    bitrate);
    config.num_frames_to_encode = kNumFramesToEncodeForPerformance;
    if (encoder_rate != 0)
      config.encode_interval =
          base::TimeDelta::FromSeconds(/*secs=*/1u) / encoder_rate;

    auto video_encoder =
        VideoEncoder::Create(config, g_env->GetGpuMemoryBufferFactory(),
                             std::move(bitstream_processors));
    LOG_ASSERT(video_encoder);
    LOG_ASSERT(video_encoder->Initialize(video));

    return video_encoder;
  }

  scoped_refptr<const VideoFrame> GetModelFrame(size_t frame_index) {
    LOG_ASSERT(raw_data_helper_);
    return raw_data_helper_->GetFrame(frame_index %
                                      g_env->Video()->NumFrames());
  }

  std::unique_ptr<RawDataHelper> raw_data_helper_;

  PerformanceEvaluator* performance_evaluator_;
  SSIMVideoFrameValidator* ssim_validator_;
  PSNRVideoFrameValidator* psnr_validator_;
};

}  // namespace

// Encode |kNumFramesToEncodeForPerformance| frames while measuring uncapped
// performance. This test will encode a video as fast as possible, and gives an
// idea about the maximum output of the encoder.
TEST_F(VideoEncoderTest, MeasureUncappedPerformance) {
  auto encoder =
      CreateVideoEncoder(g_env->Video(), g_env->Profile(), g_env->Bitrate());
  encoder->SetEventWaitTimeout(kPerfEventTimeout);

  performance_evaluator_->StartMeasuring();
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  performance_evaluator_->StopMeasuring();

  auto metrics = performance_evaluator_->Metrics();
  metrics.WriteToConsole();
  metrics.WriteToFile();

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), kNumFramesToEncodeForPerformance);
}

// Encode |kNumFramesToEncodeForPerformance| frames while measuring uncapped
// performance. This test will encode a video at a fixed ratio, 30fps.
// This test can be used to measure the cpu metrics during encoding.
TEST_F(VideoEncoderTest, MeasureCappedPerformance) {
  const uint32_t kEncodeRate = 30;
  auto encoder = CreateVideoEncoder(g_env->Video(), g_env->Profile(),
                                    g_env->Bitrate(), kEncodeRate);
  encoder->SetEventWaitTimeout(kPerfEventTimeout);

  performance_evaluator_->StartMeasuring();
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  performance_evaluator_->StopMeasuring();

  auto metrics = performance_evaluator_->Metrics();
  metrics.WriteToConsole();
  metrics.WriteToFile();

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), kNumFramesToEncodeForPerformance);
}

TEST_F(VideoEncoderTest, MeasureProducedBitstreamQuality) {
  auto encoder = CreateVideoEncoderForQualityPerformance(
      g_env->Video(), g_env->Profile(), g_env->Bitrate());
  encoder->SetEventWaitTimeout(kPerfEventTimeout);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), kNumFramesToEncodeForPerformance);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());

  BitstreamQualityMetrics metrics(*psnr_validator_, *ssim_validator_);
  metrics.WriteToConsole();
  metrics.WriteToFile();
}
}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

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
  base::FilePath video_path =
      (args.size() >= 1) ? base::FilePath(args[0])
                         : base::FilePath(media::test::kDefaultTestVideoPath);
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();
  std::string codec = "h264";

  // Parse command line arguments.
  base::FilePath::StringType output_folder = media::test::kDefaultOutputFolder;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "output_folder") {
      output_folder = it->second;
    } else if (it->first == "codec") {
      codec = it->second;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoEncoderTestEnvironment* test_environment =
      media::test::VideoEncoderTestEnvironment::Create(
          video_path, video_metadata_path, false, base::FilePath(output_folder),
          codec, 1u /* num_temporal_layers */, false /* output_bitstream */);
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
