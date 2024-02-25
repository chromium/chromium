// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace content {

namespace {

// Populates `output` and returns true on success (i.e. if `relative_path`
// exists and can be read into `output`).  Otherwise returns false.
bool ReadTestFile(const base::FilePath& relative_path,
                  std::vector<uint8_t>& output) {
  base::FilePath source_root_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir)) {
    return false;
  }

  std::string file_contents_as_string;
  {
    base::ScopedAllowBlockingForTesting allow_file_io_for_testing;
    base::FilePath absolute_path = source_root_dir.Append(relative_path);
    if (!base::ReadFileToString(absolute_path, &file_contents_as_string))
      return false;
  }

  // Convert chars to uint8_ts.
  for (const char& c : file_contents_as_string)
    output.push_back(c);

  return true;
}

// Populates `out_measurement_value` and returns true on success (i.e. if the
// `metric_name` has a single measurement in `histograms`).  Otherwise returns
// false.
bool GetSingleMeasurement(const base::HistogramTester& histograms,
                          const char* metric_name,
                          base::TimeDelta& out_measurement_value) {
  DCHECK(metric_name);

  std::vector<base::Bucket> buckets = histograms.GetAllSamples(metric_name);
  if (buckets.size() != 1u)
    return false;

  EXPECT_EQ(1u, buckets.size());
  out_measurement_value = base::Milliseconds(buckets.front().min);
  return true;
}

}  // namespace

using DataDecoderBrowserTest = ContentBrowserTest;

class ServiceProcessObserver : public ServiceProcessHost::Observer {
 public:
  ServiceProcessObserver() { ServiceProcessHost::AddObserver(this); }

  ServiceProcessObserver(const ServiceProcessObserver&) = delete;
  ServiceProcessObserver& operator=(const ServiceProcessObserver&) = delete;

  ~ServiceProcessObserver() override {
    ServiceProcessHost::RemoveObserver(this);
  }

  int instances_started() const { return instances_started_; }

  void WaitForNextLaunch() {
    launch_wait_loop_.emplace();
    launch_wait_loop_->Run();
  }

  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override {
    if (info.IsService<data_decoder::mojom::DataDecoderService>()) {
      ++instances_started_;
      if (launch_wait_loop_)
        launch_wait_loop_->Quit();
    }
  }

 private:
  std::optional<base::RunLoop> launch_wait_loop_;
  int instances_started_ = 0;
};

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, Launch) {
  ServiceProcessObserver observer;

  // Verifies that the DataDecoder client object launches a service process as
  // needed.
  data_decoder::DataDecoder decoder;

  // |GetService()| must always ensure a connection to the service on all
  // platforms, so we use it instead of a more specific API whose behavior may
  // vary across platforms.
  decoder.GetService();

  observer.WaitForNextLaunch();
  EXPECT_EQ(1, observer.instances_started());
}

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, LaunchIsolated) {
  ServiceProcessObserver observer;

  // Verifies that separate DataDecoder client objects will launch separate
  // service processes. We also bind a JsonParser interface to ensure that the
  // instances don't go idle.
  data_decoder::DataDecoder decoder1;
  mojo::Remote<data_decoder::mojom::JsonParser> parser1;
  decoder1.GetService()->BindJsonParser(parser1.BindNewPipeAndPassReceiver());
  observer.WaitForNextLaunch();
  EXPECT_EQ(1, observer.instances_started());

  data_decoder::DataDecoder decoder2;
  mojo::Remote<data_decoder::mojom::JsonParser> parser2;
  decoder2.GetService()->BindJsonParser(parser2.BindNewPipeAndPassReceiver());
  observer.WaitForNextLaunch();
  EXPECT_EQ(2, observer.instances_started());

  // Both interfaces should be connected end-to-end.
  parser1.FlushForTesting();
  parser2.FlushForTesting();
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_TRUE(parser2.is_connected());
}

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, DecodeImageIsolated) {
  std::vector<uint8_t> file_contents;
  base::FilePath content_test_data_path = GetTestDataFilePath();
  base::FilePath png_path =
      content_test_data_path.AppendASCII("site_isolation/png-corp.png");
  ASSERT_TRUE(ReadTestFile(png_path, file_contents));

  base::HistogramTester histograms;
  {
    base::RunLoop run_loop;
    data_decoder::DecodeImageCallback callback =
        base::BindLambdaForTesting([&](const SkBitmap& decoded_bitmap) {
          EXPECT_EQ(100, decoded_bitmap.width());
          EXPECT_EQ(100, decoded_bitmap.height());
          run_loop.Quit();
        });
    data_decoder::DecodeImageIsolated(
        file_contents, data_decoder::mojom::ImageCodec::kDefault,
        false,                                 // shrink_to_fit
        std::numeric_limits<uint32_t>::max(),  // max_size_in_bytes
        gfx::Size(),                           // desired_image_frame_size
        std::move(callback));
    run_loop.Run();
  }

  FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix("Security.DataDecoder"),
      UnorderedElementsAre(
          Pair("Security.DataDecoder.Image.Isolated.EndToEndTime", 1),
          Pair("Security.DataDecoder.Image.Isolated.ProcessOverhead", 1),
          Pair("Security.DataDecoder.Image.DecodingTime", 1)));

  base::TimeDelta end_to_end_duration_estimate;
  EXPECT_TRUE(GetSingleMeasurement(
      histograms, "Security.DataDecoder.Image.Isolated.EndToEndTime",
      end_to_end_duration_estimate));

  base::TimeDelta overhead_estimate;
  EXPECT_TRUE(GetSingleMeasurement(
      histograms, "Security.DataDecoder.Image.Isolated.ProcessOverhead",
      overhead_estimate));

  base::TimeDelta decoding_duration_estimate;
  EXPECT_TRUE(GetSingleMeasurement(histograms,
                                   "Security.DataDecoder.Image.DecodingTime",
                                   decoding_duration_estimate));

  EXPECT_LE(decoding_duration_estimate, end_to_end_duration_estimate);
  EXPECT_LE(overhead_estimate, end_to_end_duration_estimate);
}

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, DecodeImage) {
  std::vector<uint8_t> file_contents;
  base::FilePath content_test_data_path = GetTestDataFilePath();
  base::FilePath png_path =
      content_test_data_path.AppendASCII("site_isolation/png-corp.png");
  ASSERT_TRUE(ReadTestFile(png_path, file_contents));

  base::HistogramTester histograms;
  {
    base::RunLoop run_loop;
    data_decoder::DecodeImageCallback callback =
        base::BindLambdaForTesting([&](const SkBitmap& decoded_bitmap) {
          EXPECT_EQ(100, decoded_bitmap.width());
          EXPECT_EQ(100, decoded_bitmap.height());
          run_loop.Quit();
        });

    data_decoder::DataDecoder decoder;
    data_decoder::DecodeImage(
        &decoder, file_contents, data_decoder::mojom::ImageCodec::kDefault,
        false,                                 // shrink_to_fit
        std::numeric_limits<uint32_t>::max(),  // max_size_in_bytes
        gfx::Size(),                           // desired_image_frame_size
        std::move(callback));
    run_loop.Run();
  }

  FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix("Security.DataDecoder"),
      UnorderedElementsAre(
          Pair("Security.DataDecoder.Image.Reusable.EndToEndTime", 1),
          Pair("Security.DataDecoder.Image.Reusable.ProcessOverhead", 1),
          Pair("Security.DataDecoder.Image.DecodingTime", 1)));

  base::TimeDelta end_to_end_duration_estimate;
  EXPECT_TRUE(GetSingleMeasurement(
      histograms, "Security.DataDecoder.Image.Reusable.EndToEndTime",
      end_to_end_duration_estimate));

  base::TimeDelta overhead_estimate;
  EXPECT_TRUE(GetSingleMeasurement(
      histograms, "Security.DataDecoder.Image.Reusable.ProcessOverhead",
      overhead_estimate));

  base::TimeDelta decoding_duration_estimate;
  EXPECT_TRUE(GetSingleMeasurement(histograms,
                                   "Security.DataDecoder.Image.DecodingTime",
                                   decoding_duration_estimate));

  EXPECT_LE(decoding_duration_estimate, end_to_end_duration_estimate);
  EXPECT_LE(overhead_estimate, end_to_end_duration_estimate);
}

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest,
                       NoCallbackAfterDestruction_Json) {
  base::RunLoop run_loop;

  auto decoder = std::make_unique<data_decoder::DataDecoder>();
  auto* raw_decoder = decoder.get();

  // Android's in-process parser can complete synchronously, so queue the
  // delete task first unlike in the other tests.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(decoder));

  bool got_callback = false;
  raw_decoder->ParseJson(
      "[1, 2, 3]",
      base::BindOnce(
          [](bool* got_callback, base::ScopedClosureRunner quit_closure_runner,
             data_decoder::DataDecoder::ValueOrError result) {
            *got_callback = true;
          },
          // Pass the quit closure as a ScopedClosureRunner, so that the loop
          // is quit if the callback is destroyed un-run or after it runs.
          &got_callback, base::ScopedClosureRunner(run_loop.QuitClosure())));

  run_loop.Run();

  EXPECT_FALSE(got_callback);
}

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, NoCallbackAfterDestruction_Xml) {
  base::RunLoop run_loop;

  auto decoder = std::make_unique<data_decoder::DataDecoder>();
  bool got_callback = false;
  decoder->ParseXml(
      "<marquee>hello world</marquee>",
      data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(
          [](bool* got_callback, base::ScopedClosureRunner quit_closure_runner,
             data_decoder::DataDecoder::ValueOrError result) {
            *got_callback = true;
          },
          // Pass the quit closure as a ScopedClosureRunner, so that the loop
          // is quit if the callback is destroyed un-run or after it runs.
          &got_callback, base::ScopedClosureRunner(run_loop.QuitClosure())));

  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(decoder));
  run_loop.Run();

  EXPECT_FALSE(got_callback);
}

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest,
                       NoCallbackAfterDestruction_Gzip) {
  base::RunLoop run_loop;

  auto decoder = std::make_unique<data_decoder::DataDecoder>();
  bool got_callback = false;
  decoder->GzipCompress(
      {{0x1, 0x1, 0x1, 0x1, 0x1, 0x1}},
      base::BindOnce(
          [](bool* got_callback, base::ScopedClosureRunner quit_closure_runner,
             base::expected<mojo_base::BigBuffer, std::string> result) {
            *got_callback = true;
          },
          // Pass the quit closure as a ScopedClosureRunner, so that the loop
          // is quit if the callback is destroyed un-run or after it runs.
          &got_callback, base::ScopedClosureRunner(run_loop.QuitClosure())));

  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(decoder));
  run_loop.Run();

  EXPECT_FALSE(got_callback);
}

}  // namespace content
