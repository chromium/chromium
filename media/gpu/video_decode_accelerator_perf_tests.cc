// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <numeric>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video decoder perf tests usage message. Make sure to also update the
// documentation under docs/media/gpu/video_decoder_perf_test_usage.md when
// making changes here.
constexpr const char* usage_msg =
    "usage: video_decode_accelerator_perf_tests\n"
    "           [-v=<level>] [--vmodule=<config>] [--output_folder]\n"
    "           [--use_vd] [--gtest_help] [--help]\n"
    "           [<video path>] [<video metadata path>]\n";

// Video decoder perf tests help message.
constexpr const char* help_msg =
    "Run the video decode accelerator performance tests on the video\n"
    "specified by <video path>. If no <video path> is given the default\n"
    "\"test-25fps.h264\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata, such as frame checksums. By default\n"
    "<video path>.json will be used.\n"
    "\nThe following arguments are supported:\n"
    "   -v                  enable verbose mode, e.g. -v=2.\n"
    "  --vmodule            enable verbose mode for the specified module,\n"
    "                       e.g. --vmodule=*media/gpu*=2.\n"
    "  --output_folder      overwrite the output folder used to store\n"
    "                       performance metrics, if not specified results\n"
    "                       will be stored in the current working directory.\n"
    "  --use_vd             use the new VD-based video decoders, instead of\n"
    "                       the default VDA-based video decoders.\n"
    "  --gtest_help         display the gtest help and exit.\n"
    "  --help               display this help and exit.\n";

media::test::VideoPlayerTestEnvironment* g_env;

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
  avg_ms_ = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
  std::vector<double> sorted_times = times;
  std::sort(sorted_times.begin(), sorted_times.end());
  percentile_25_ms_ = sorted_times[sorted_times.size() / 4];
  percentile_50_ms_ = sorted_times[sorted_times.size() / 2];
  percentile_75_ms_ = sorted_times[(sorted_times.size() * 3) / 4];
}

struct PerformanceMetrics {
  // Total measurement duration.
  base::TimeDelta total_duration_;
  // The number of frames decoded.
  size_t frames_decoded_ = 0;
  // The overall number of frames decoded per second.
  double frames_per_second_ = 0.0;
  // The number of frames dropped because of the decoder running behind, only
  // relevant for capped performance tests.
  size_t frames_dropped_ = 0;
  // The percentage of frames dropped because of the decoder running behind,
  // only relevant for capped performance tests.
  double dropped_frame_percentage_ = 0.0;
  // Statistics about the time between subsequent frame deliveries.
  PerformanceTimeStats delivery_time_stats_;
  // Statistics about the time between decode start and frame deliveries.
  PerformanceTimeStats decode_time_stats_;
};

// The performance evaluator can be plugged into the video player to collect
// various performance metrics.
// TODO(dstaessens@) Check and post warning when CPU frequency scaling is
// enabled as this affects test results.
class PerformanceEvaluator : public VideoFrameProcessor {
 public:
  // Create a new performance evaluator. The caller should makes sure
  // |frame_renderer| outlives the performance evaluator.
  explicit PerformanceEvaluator(const FrameRendererDummy* const frame_renderer)
      : frame_renderer_(frame_renderer) {}

  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) override;
  bool WaitUntilDone() override { return true; }

  // Start/Stop collecting performance metrics.
  void StartMeasuring();
  void StopMeasuring();

  // Write the collected performance metrics to file.
  void WriteMetricsToFile() const;

 private:
  // Start/end time of the measurement period.
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;

  // Time at which the previous frame was delivered.
  base::TimeTicks prev_frame_delivery_time_;
  // List of times between subsequent frame deliveries.
  std::vector<double> frame_delivery_times_;
  // List of times between decode start and frame delivery.
  std::vector<double> frame_decode_times_;

  // Collection of various performance metrics.
  PerformanceMetrics perf_metrics_;

  // Frame renderer used to get the dropped frame rate, owned by the creator of
  // the performance evaluator.
  const FrameRendererDummy* const frame_renderer_;
};

void PerformanceEvaluator::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta delivery_time = (now - prev_frame_delivery_time_);
  frame_delivery_times_.push_back(delivery_time.InMillisecondsF());
  prev_frame_delivery_time_ = now;

  base::TimeDelta decode_time = now.since_origin() - video_frame->timestamp();
  frame_decode_times_.push_back(decode_time.InMillisecondsF());

  perf_metrics_.frames_decoded_++;
}

void PerformanceEvaluator::StartMeasuring() {
  start_time_ = base::TimeTicks::Now();
  prev_frame_delivery_time_ = start_time_;
}

void PerformanceEvaluator::StopMeasuring() {
  end_time_ = base::TimeTicks::Now();
  perf_metrics_.total_duration_ = end_time_ - start_time_;
  perf_metrics_.frames_per_second_ = perf_metrics_.frames_decoded_ /
                                     perf_metrics_.total_duration_.InSecondsF();
  perf_metrics_.frames_dropped_ = frame_renderer_->FramesDropped();

  // Calculate the dropped frame percentage.
  perf_metrics_.dropped_frame_percentage_ =
      static_cast<double>(perf_metrics_.frames_dropped_) /
      static_cast<double>(
          std::max<size_t>(perf_metrics_.frames_decoded_, 1ul)) *
      100.0;

  // Calculate delivery and decode time metrics.
  perf_metrics_.delivery_time_stats_ =
      PerformanceTimeStats(frame_delivery_times_);
  perf_metrics_.decode_time_stats_ = PerformanceTimeStats(frame_decode_times_);

  std::cout << "Frames decoded:     " << perf_metrics_.frames_decoded_
            << std::endl;
  std::cout << "Total duration:     "
            << perf_metrics_.total_duration_.InMillisecondsF() << "ms"
            << std::endl;
  std::cout << "FPS:                " << perf_metrics_.frames_per_second_
            << std::endl;
  std::cout << "Frames Dropped:     " << perf_metrics_.frames_dropped_
            << std::endl;
  std::cout << "Dropped frame percentage: "
            << perf_metrics_.dropped_frame_percentage_ << "%" << std::endl;
  std::cout << "Frame delivery time - average:       "
            << perf_metrics_.delivery_time_stats_.avg_ms_ << "ms" << std::endl;
  std::cout << "Frame delivery time - percentile 25: "
            << perf_metrics_.delivery_time_stats_.percentile_25_ms_ << "ms"
            << std::endl;
  std::cout << "Frame delivery time - percentile 50: "
            << perf_metrics_.delivery_time_stats_.percentile_50_ms_ << "ms"
            << std::endl;
  std::cout << "Frame delivery time - percentile 75: "
            << perf_metrics_.delivery_time_stats_.percentile_75_ms_ << "ms"
            << std::endl;
  std::cout << "Frame decode time - average:       "
            << perf_metrics_.decode_time_stats_.avg_ms_ << "ms" << std::endl;
  std::cout << "Frame decode time - percentile 25: "
            << perf_metrics_.decode_time_stats_.percentile_25_ms_ << "ms"
            << std::endl;
  std::cout << "Frame decode time - percentile 50: "
            << perf_metrics_.decode_time_stats_.percentile_50_ms_ << "ms"
            << std::endl;
  std::cout << "Frame decode time - percentile 75: "
            << perf_metrics_.decode_time_stats_.percentile_75_ms_ << "ms"
            << std::endl;
}

void PerformanceEvaluator::WriteMetricsToFile() const {
  base::FilePath output_folder_path = base::FilePath(g_env->OutputFolder());
  if (!DirectoryExists(output_folder_path))
    base::CreateDirectory(output_folder_path);
  output_folder_path = base::MakeAbsoluteFilePath(output_folder_path);

  // Write performance metrics to json.
  base::Value metrics(base::Value::Type::DICTIONARY);
  metrics.SetKey(
      "FramesDecoded",
      base::Value(base::checked_cast<int>(perf_metrics_.frames_decoded_)));
  metrics.SetKey("TotalDurationMs",
                 base::Value(perf_metrics_.total_duration_.InMillisecondsF()));
  metrics.SetKey("FPS", base::Value(perf_metrics_.frames_per_second_));
  metrics.SetKey(
      "FramesDropped",
      base::Value(base::checked_cast<int>(perf_metrics_.frames_dropped_)));
  metrics.SetKey("DroppedFramePercentage",
                 base::Value(perf_metrics_.dropped_frame_percentage_));
  metrics.SetKey("FrameDeliveryTimeAverage",
                 base::Value(perf_metrics_.delivery_time_stats_.avg_ms_));
  metrics.SetKey(
      "FrameDeliveryTimePercentile25",
      base::Value(perf_metrics_.delivery_time_stats_.percentile_25_ms_));
  metrics.SetKey(
      "FrameDeliveryTimePercentile50",
      base::Value(perf_metrics_.delivery_time_stats_.percentile_50_ms_));
  metrics.SetKey(
      "FrameDeliveryTimePercentile75",
      base::Value(perf_metrics_.delivery_time_stats_.percentile_75_ms_));
  metrics.SetKey("FrameDecodeTimeAverage",
                 base::Value(perf_metrics_.decode_time_stats_.avg_ms_));
  metrics.SetKey(
      "FrameDecodeTimePercentile25",
      base::Value(perf_metrics_.decode_time_stats_.percentile_25_ms_));
  metrics.SetKey(
      "FrameDecodeTimePercentile50",
      base::Value(perf_metrics_.decode_time_stats_.percentile_50_ms_));
  metrics.SetKey(
      "FrameDecodeTimePercentile75",
      base::Value(perf_metrics_.decode_time_stats_.percentile_75_ms_));

  // Write frame delivery times to json.
  base::Value delivery_times(base::Value::Type::LIST);
  for (double frame_delivery_time : frame_delivery_times_) {
    delivery_times.Append(frame_delivery_time);
  }
  metrics.SetKey("FrameDeliveryTimes", std::move(delivery_times));

  // Write frame decodes times to json.
  base::Value decode_times(base::Value::Type::LIST);
  for (double frame_decode_time : frame_decode_times_) {
    decode_times.Append(frame_decode_time);
  }
  metrics.SetKey("FrameDecodeTimes", std::move(decode_times));

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

// Video decode test class. Performs setup and teardown for each single test.
class VideoDecoderTest : public ::testing::Test {
 public:
  // Create a new video player instance. |render_frame_rate| is the rate at
  // which the video player will simulate rendering frames, if 0 no rendering is
  // simulated. The |vsync_rate| is used during simulated rendering, if 0 Vsync
  // is disabled.
  std::unique_ptr<VideoPlayer> CreateVideoPlayer(const Video* video,
                                                 uint32_t render_frame_rate = 0,
                                                 uint32_t vsync_rate = 0) {
    LOG_ASSERT(video);

    // Create dummy frame renderer, simulates rendering at specified frame rate.
    base::TimeDelta frame_duration;
    base::TimeDelta vsync_interval_duration;
    if (render_frame_rate > 0) {
      frame_duration = base::TimeDelta::FromSeconds(1) / render_frame_rate;
      vsync_interval_duration = base::TimeDelta::FromSeconds(1) / vsync_rate;
    }
    auto frame_renderer =
        FrameRendererDummy::Create(frame_duration, vsync_interval_duration);

    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors;
    auto performance_evaluator =
        std::make_unique<PerformanceEvaluator>(frame_renderer.get());
    performance_evaluator_ = performance_evaluator.get();
    frame_processors.push_back(std::move(performance_evaluator));

    // Use the new VD-based video decoders if requested.
    VideoDecoderClientConfig config;
    config.use_vd = g_env->UseVD();

    // Force allocate mode if import mode is not supported.
    if (!g_env->ImportSupported())
      config.allocation_mode = AllocationMode::kAllocate;

    auto video_player = VideoPlayer::Create(
        config, g_env->GetGpuMemoryBufferFactory(), std::move(frame_renderer),
        std::move(frame_processors));
    LOG_ASSERT(video_player);
    LOG_ASSERT(video_player->Initialize(video));

    // Make sure the event timeout is at least as long as the video's duration.
    video_player->SetEventWaitTimeout(
        std::max(kDefaultEventWaitTimeout, g_env->Video()->GetDuration()));
    return video_player;
  }

  PerformanceEvaluator* performance_evaluator_;
};

}  // namespace

// Play video from start to end while measuring uncapped performance. This test
// will decode a video as fast as possible, and gives an idea about the maximum
// output of the decoder.
TEST_F(VideoDecoderTest, MeasureUncappedPerformance) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  performance_evaluator_->StartMeasuring();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  performance_evaluator_->StopMeasuring();
  performance_evaluator_->WriteMetricsToFile();

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
}

// Play video from start to end while measuring capped performance. This test
// will simulate rendering the video at its actual frame rate, and will
// calculate the number of frames that were dropped. Vsync is enabled at 60 FPS.
TEST_F(VideoDecoderTest, MeasureCappedPerformance) {
  auto tvp = CreateVideoPlayer(g_env->Video(), g_env->Video()->FrameRate(), 60);

  performance_evaluator_->StartMeasuring();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->WaitForRenderer();
  performance_evaluator_->StopMeasuring();
  performance_evaluator_->WriteMetricsToFile();

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
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
      (args.size() >= 1) ? base::FilePath(args[0]) : base::FilePath();
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();

  // Parse command line arguments.
  base::FilePath::StringType output_folder = media::test::kDefaultOutputFolder;
  bool use_vd = false;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "output_folder") {
      output_folder = it->second;
    } else if (it->first == "use_vd") {
      use_vd = true;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoPlayerTestEnvironment* test_environment =
      media::test::VideoPlayerTestEnvironment::Create(
          video_path, video_metadata_path, false, use_vd,
          base::FilePath(output_folder));
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoPlayerTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
