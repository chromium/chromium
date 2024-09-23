// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/map_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "services/screen_ai/screen_ai_library_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

#if BUILDFLAG(USE_FAKE_SCREEN_AI)
#include "services/screen_ai/screen_ai_library_wrapper_fake.h"
#else
#include "services/screen_ai/screen_ai_library_wrapper_impl.h"
#endif

namespace screen_ai {

namespace {

constexpr char kUsageMessage[] =
    R"(Usage:
  screen_ai_ocr_perf [options]

Options:
  --help        Show this help message and exit.
  --jpeg_image  The image to test in JPEG format.
  --output_path The path to store the perf result in JSON format.
)";

constexpr char kJpegImageOption[] = "jpeg_image";
constexpr char kOutputPathOption[] = "output_path";

constexpr int kWarmUpIterationCount = 3;
constexpr int kActualIterationCount = 5;

base::FilePath kLibraryDirectoryPath = GetComponentDir();
base::FilePath kLibraryName = GetComponentBinaryFileName();
// The name of the file that contains a list of files that are required to
// initialize the library. The file paths are separated by newlines and
// relative to `kLibraryDirectoryPath`.
constexpr base::FilePath::CharType kFilePathsFileName[] =
    FILE_PATH_LITERAL("files_list_ocr.txt");

SkBitmap GetBitmap(const base::FilePath& path) {
  auto data = base::ReadFileToBytes(path);
  CHECK(data);
  std::unique_ptr<SkBitmap> bitmap =
      gfx::JPEGCodec::Decode(data->data(), data->size());
  return std::move(*bitmap);
}

class OcrTestEnvironment : public ::testing::Environment {
 public:
  // This is static since`ScreenAILibraryWrapperImpl` uses function pointers
  // to access data, and there's no way to pass context data through these
  // pointers. It stores the model data the library needs. Keys are the file
  // paths relative to the library directory, and values are the corresponding
  // buffers.
  static inline std::map<std::string, std::vector<uint8_t>> data_;

  static uint32_t GetDataSize(const char* relative_file_path) {
    auto* data =
        base::FindOrNull(OcrTestEnvironment::data_, relative_file_path);
    CHECK(data);
    return data->size();
  }

  static void CopyData(const char* relative_file_path,
                       uint32_t buffer_size,
                       char* buffer) {
    auto* data =
        base::FindOrNull(OcrTestEnvironment::data_, relative_file_path);
    CHECK(data);
    CHECK_GE(buffer_size, data->size());
    memcpy(buffer, data->data(), data->size());
  }

  OcrTestEnvironment(const std::string& output_path,
                     const std::string& jpeg_image_path)
      : output_path_(output_path),
        jpeg_image_(GetBitmap(base::FilePath(jpeg_image_path))) {}

  void SetUp() override {
    CHECK(!jpeg_image_.empty());

    base::FilePath directory_path(kLibraryDirectoryPath);
    base::FilePath library_path = directory_path.Append(kLibraryName);
#if BUILDFLAG(USE_FAKE_SCREEN_AI)
    library_ = std::make_unique<ScreenAILibraryWrapperFake>();
#else
    library_ = std::make_unique<ScreenAILibraryWrapperImpl>();
#endif
    CHECK(library_->Load(library_path))
        << "Run `dlcservice_util --id=screen-ai --install` to install the lib.";

    library_->SetFileContentFunctions(&OcrTestEnvironment::GetDataSize,
                                      &OcrTestEnvironment::CopyData);

    InitOcr();
  }

  void InitOcr() {
    PrepareModelData();
    CHECK(library_->InitOCR());
  }

  void PrepareModelData() {
    base::FilePath directory_path(kLibraryDirectoryPath);
    base::FilePath file_paths_path = directory_path.Append(kFilePathsFileName);
    std::string file_content;
    CHECK(base::ReadFileToString(file_paths_path, &file_content))
        << "Could not read list of files for " << kFilePathsFileName;

    std::vector<std::string_view> files_list = base::SplitStringPiece(
        file_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK(!files_list.empty())
        << "Could not parse files list for " << kFilePathsFileName;
    for (auto& relative_file_path : files_list) {
      // Ignore comment lines.
      if (relative_file_path.empty() || relative_file_path[0] == '#') {
        continue;
      }
      std::optional<std::vector<uint8_t>> buffer =
          base::ReadFileToBytes(directory_path.Append(relative_file_path));
      CHECK(buffer) << "Could not read file's content: " << relative_file_path;
      auto [it, inserted] = OcrTestEnvironment::data_.insert(
          {std::string(relative_file_path), std::move(*buffer)});
      CHECK(inserted);
    }
  }

  void PerformOcr() { library_->PerformOcr(jpeg_image_); }

  void Benchmark(const std::string& metrics_name,
                 base::RepeatingClosure target_ops) {
    base::LapTimer lap_timer(kWarmUpIterationCount, base::Seconds(10), 1);
    do {
      target_ops.Run();
      lap_timer.NextLap();
    } while (lap_timer.NumLaps() < kActualIterationCount);

    int avg_duration = lap_timer.TimePerLap().InMilliseconds();
    perf_values_.Set(metrics_name, avg_duration);
    LOG(INFO) << "Perf: " << metrics_name << " => " << avg_duration << " ms";
  }

  void TearDown() override {
    JSONFileValueSerializer json_serializer(output_path_);
    CHECK(json_serializer.Serialize(perf_values_));
  }

  base::Value::Dict perf_values_;
  base::FilePath output_path_;
  SkBitmap jpeg_image_;
  std::unique_ptr<ScreenAILibraryWrapper> library_;
};

OcrTestEnvironment* g_env;

class OcrPerfTest : public ::testing::Test {
 protected:
  OcrPerfTest() = default;
};

}  // namespace

TEST_F(OcrPerfTest, PerformOcrPerf) {
  g_env->Benchmark("PerformOcr",
                   base::BindLambdaForTesting([&]() { g_env->PerformOcr(); }));
}

}  // namespace screen_ai

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (cmd_line->HasSwitch("help")) {
    LOG(INFO) << screen_ai::kUsageMessage;
    return 0;
  }

  std::string jpeg_image =
      cmd_line->GetSwitchValueASCII(screen_ai::kJpegImageOption);
  std::string output_path =
      cmd_line->GetSwitchValueASCII(screen_ai::kOutputPathOption);

  if (jpeg_image.empty() || output_path.empty()) {
    LOG(ERROR) << "Missing required options: " << screen_ai::kJpegImageOption
               << ", " << screen_ai::kOutputPathOption << "\n";
    return EXIT_FAILURE;
  }

  ::testing::InitGoogleTest(&argc, argv);

  screen_ai::g_env = new screen_ai::OcrTestEnvironment(output_path, jpeg_image);
  ::testing::AddGlobalTestEnvironment(screen_ai::g_env);
  return RUN_ALL_TESTS();
}
