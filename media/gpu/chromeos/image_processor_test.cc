// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/test/image.h"
#include "media/gpu/test/image_processor/image_processor_client.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_test_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

const char* usage_msg =
    "usage: image_processor_test\n"
    "[--gtest_help] [--help] [-v=<level>] [--vmodule=<config>] "
    "[--save_images]\n";

const char* help_msg =
    "Run the image processor tests.\n\n"
    "The following arguments are supported:\n"
    "  --gtest_help          display the gtest help and exit.\n"
    "  --help                display this help and exit.\n"
    "   -v                   enable verbose mode, e.g. -v=2.\n"
    "  --vmodule             enable verbose mode for the specified module.\n"
    "  --save_images         write images processed by a image processor to\n"
    "                        the \"<testname>\" folder.\n";

bool g_save_images = false;

media::test::VideoTestEnvironment* g_env;

// Files for pixel format conversion test.
// TODO(crbug.com/944822): Use kI420Image for I420 -> NV12 test case. It is
// currently disabled because there is currently no way of creating DMABUF I420
// buffer by NativePixmap.
// constexpr const base::FilePath::CharType* kI420Image =
//     FILE_PATH_LITERAL("bear_320x192.i420.yuv");
const base::FilePath::CharType* kNV12Image =
    FILE_PATH_LITERAL("bear_320x192.nv12.yuv");
const base::FilePath::CharType* kRGBAImage =
    FILE_PATH_LITERAL("bear_320x192.rgba");
const base::FilePath::CharType* kBGRAImage =
    FILE_PATH_LITERAL("bear_320x192.bgra");
const base::FilePath::CharType* kYV12Image =
    FILE_PATH_LITERAL("bear_320x192.yv12.yuv");

// Files for scaling test.
const base::FilePath::CharType* kNV12Image720P =
    FILE_PATH_LITERAL("puppets-1280x720.nv12.yuv");
const base::FilePath::CharType* kNV12Image360P =
    FILE_PATH_LITERAL("puppets-640x360.nv12.yuv");
const base::FilePath::CharType* kNV12Image180P =
    FILE_PATH_LITERAL("puppets-320x180.nv12.yuv");

class ImageProcessorParamTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<base::FilePath, base::FilePath>> {
 public:
  void SetUp() override {}
  void TearDown() override {}

  std::unique_ptr<test::ImageProcessorClient> CreateImageProcessorClient(
      const test::Image& input_image,
      const std::vector<VideoFrame::StorageType>& input_storage_types,
      test::Image* const output_image,
      const std::vector<VideoFrame::StorageType>& output_storage_types) {
    Fourcc input_fourcc =
        Fourcc::FromVideoPixelFormat(input_image.PixelFormat());
    Fourcc output_fourcc =
        Fourcc::FromVideoPixelFormat(output_image->PixelFormat());
    auto input_layout = test::CreateVideoFrameLayout(input_image.PixelFormat(),
                                                     input_image.Size());
    auto output_layout = test::CreateVideoFrameLayout(
        output_image->PixelFormat(), output_image->Size());
    LOG_ASSERT(input_layout && output_layout);
    ImageProcessor::PortConfig input_config(
        input_fourcc, input_image.Size(), input_layout->planes(),
        input_image.Size(), input_storage_types);
    ImageProcessor::PortConfig output_config(
        output_fourcc, output_image->Size(), output_layout->planes(),
        output_image->Size(), output_storage_types);
    // TODO(crbug.com/917951): Select more appropriate number of buffers.
    constexpr size_t kNumBuffers = 1;
    LOG_ASSERT(output_image->IsMetadataLoaded());
    std::vector<std::unique_ptr<test::VideoFrameProcessor>> frame_processors;
    // TODO(crbug.com/944823): Use VideoFrameValidator for RGB formats.
    // TODO(crbug.com/917951): We should validate a scaled image with SSIM.
    // Validating processed frames is currently not supported when a format is
    // not YUV or when scaling images.
    if (IsYuvPlanar(input_fourcc.ToVideoPixelFormat()) &&
        IsYuvPlanar(output_fourcc.ToVideoPixelFormat())) {
      if (input_image.Size() == output_image->Size()) {
        auto vf_validator = test::VideoFrameValidator::Create(
            {output_image->Checksum()}, output_image->PixelFormat());
        frame_processors.push_back(std::move(vf_validator));
      } else if (input_fourcc == output_fourcc) {
        // Scaling case.
        LOG_ASSERT(output_image->Load());
        scoped_refptr<const VideoFrame> model_frame =
            CreateVideoFrameFromImage(*output_image);
        LOG_ASSERT(model_frame) << "Failed to create from image";
        // Scaling is not deterministic process. There are various algorithms to
        // scale images. We set a weaker tolerance value, 32, to avoid false
        // negative.
        constexpr uint32_t kImageProcessorTestTorelance = 32;
        auto vf_validator = test::VideoFrameValidator::Create(
            {model_frame}, kImageProcessorTestTorelance);
        frame_processors.push_back(std::move(vf_validator));
      }
    }

    if (g_save_images) {
      base::FilePath output_dir =
          base::FilePath(base::FilePath::kCurrentDirectory)
              .Append(g_env->GetTestOutputFilePath());
      test::VideoFrameFileWriter::OutputFormat saved_file_format =
          IsYuvPlanar(output_fourcc.ToVideoPixelFormat())
              ? test::VideoFrameFileWriter::OutputFormat::kYUV
              : test::VideoFrameFileWriter::OutputFormat::kPNG;
      frame_processors.push_back(
          test::VideoFrameFileWriter::Create(output_dir, saved_file_format));
    }

    auto ip_client = test::ImageProcessorClient::Create(
        input_config, output_config, kNumBuffers, std::move(frame_processors));
    LOG_ASSERT(ip_client) << "Failed to create ImageProcessorClient";
    return ip_client;
  }
};

TEST_P(ImageProcessorParamTest, ConvertOneTime_MemToMem) {
  // Load the test input image. We only need the output image's metadata so we
  // can compare checksums.
  test::Image input_image(std::get<0>(GetParam()));
  test::Image output_image(std::get<1>(GetParam()));
  ASSERT_TRUE(input_image.Load());
  ASSERT_TRUE(output_image.LoadMetadata());
  if (input_image.PixelFormat() == output_image.PixelFormat()) {
    // If the input format is the same as the output format, then the conversion
    // is scaling. LibyuvImageProcessor doesn't support scaling yet. So skip
    // this test case.
    // TODO(hiroh): Remove this skip once LibyuvIP supports scaling.
    GTEST_SKIP();
  }

  auto ip_client = CreateImageProcessorClient(
      input_image, {VideoFrame::STORAGE_OWNED_MEMORY}, &output_image,
      {VideoFrame::STORAGE_OWNED_MEMORY});

  ip_client->Process(input_image, output_image);

  EXPECT_TRUE(ip_client->WaitUntilNumImageProcessed(1u));
  EXPECT_EQ(ip_client->GetErrorCount(), 0u);
  EXPECT_EQ(ip_client->GetNumOfProcessedImages(), 1u);
  EXPECT_TRUE(ip_client->WaitForFrameProcessors());
}

#if defined(OS_CHROMEOS)
// We don't yet have the function to create Dmabuf-backed VideoFrame on
// platforms except ChromeOS. So MemToDmabuf test is limited on ChromeOS.
TEST_P(ImageProcessorParamTest, ConvertOneTime_DmabufToMem) {
  // Load the test input image. We only need the output image's metadata so we
  // can compare checksums.
  test::Image input_image(std::get<0>(GetParam()));
  test::Image output_image(std::get<1>(GetParam()));
  ASSERT_TRUE(input_image.Load());
  ASSERT_TRUE(output_image.LoadMetadata());
  if (input_image.PixelFormat() == output_image.PixelFormat()) {
    // If the input format is the same as the output format, then the conversion
    // is scaling. LibyuvImageProcessor doesn't support scaling yet. So skip
    // this test case.
    // TODO(hiroh): Remove this skip once LibyuvIP supports scaling.
    GTEST_SKIP();
  }

  auto ip_client = CreateImageProcessorClient(
      input_image, {VideoFrame::STORAGE_DMABUFS}, &output_image,
      {VideoFrame::STORAGE_OWNED_MEMORY});

  ip_client->Process(input_image, output_image);

  EXPECT_TRUE(ip_client->WaitUntilNumImageProcessed(1u));
  EXPECT_EQ(ip_client->GetErrorCount(), 0u);
  EXPECT_EQ(ip_client->GetNumOfProcessedImages(), 1u);
  EXPECT_TRUE(ip_client->WaitForFrameProcessors());
}

TEST_P(ImageProcessorParamTest, ConvertOneTime_DmabufToDmabuf) {
  // Load the test input image. We only need the output image's metadata so we
  // can compare checksums.
  test::Image input_image(std::get<0>(GetParam()));
  test::Image output_image(std::get<1>(GetParam()));
  ASSERT_TRUE(input_image.Load());
  ASSERT_TRUE(output_image.LoadMetadata());

  auto ip_client =
      CreateImageProcessorClient(input_image, {VideoFrame::STORAGE_DMABUFS},
                                 &output_image, {VideoFrame::STORAGE_DMABUFS});

  ip_client->Process(input_image, output_image);

  EXPECT_TRUE(ip_client->WaitUntilNumImageProcessed(1u));
  EXPECT_EQ(ip_client->GetErrorCount(), 0u);
  EXPECT_EQ(ip_client->GetNumOfProcessedImages(), 1u);
  EXPECT_TRUE(ip_client->WaitForFrameProcessors());
}

// Although GpuMemoryBuffer is a cross platform class, code for image processor
// test is designed only for ChromeOS. So this test runs on ChromeOS only.
TEST_P(ImageProcessorParamTest, ConvertOneTime_GmbToGmb) {
  // Load the test input image. We only need the output image's metadata so we
  // can compare checksums.
  test::Image input_image(std::get<0>(GetParam()));
  test::Image output_image(std::get<1>(GetParam()));
  ASSERT_TRUE(input_image.Load());
  ASSERT_TRUE(output_image.LoadMetadata());

  auto ip_client = CreateImageProcessorClient(
      input_image, {VideoFrame::STORAGE_GPU_MEMORY_BUFFER}, &output_image,
      {VideoFrame::STORAGE_GPU_MEMORY_BUFFER});

  ip_client->Process(input_image, output_image);

  EXPECT_TRUE(ip_client->WaitUntilNumImageProcessed(1u));
  EXPECT_EQ(ip_client->GetErrorCount(), 0u);
  EXPECT_EQ(ip_client->GetNumOfProcessedImages(), 1u);
  EXPECT_TRUE(ip_client->WaitForFrameProcessors());
}
#endif  // defined(OS_CHROMEOS)

// BGRA -> NV12
// I420 -> NV12
// RGBA -> NV12
// YV12 -> NV12
INSTANTIATE_TEST_SUITE_P(
    PixelFormatConversionToNV12,
    ImageProcessorParamTest,
    ::testing::Values(std::make_tuple(kBGRAImage, kNV12Image),
                      // TODO(crbug.com/944822): Add I420 -> NV12 test case.
                      // There is currently no way of creating DMABUF
                      // I420 buffer by NativePixmap.
                      // std::make_tuple(kI420Image, kNV12Image),
                      std::make_tuple(kRGBAImage, kNV12Image),
                      std::make_tuple(kYV12Image, kNV12Image)));

INSTANTIATE_TEST_SUITE_P(
    NV12DownScaling,
    ImageProcessorParamTest,
    ::testing::Values(std::make_tuple(kNV12Image720P, kNV12Image360P),
                      std::make_tuple(kNV12Image720P, kNV12Image180P),
                      std::make_tuple(kNV12Image360P, kNV12Image180P)));

#if defined(OS_CHROMEOS)
// TODO(hiroh): Add more tests.
// MEM->DMABUF (V4L2VideoEncodeAccelerator),
#endif
}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::usage_msg << "\n" << media::help_msg;
    return 0;
  }

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "save_images") {
      media::g_save_images = true;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  auto* const test_environment = new media::test::VideoTestEnvironment;
  media::g_env = reinterpret_cast<media::test::VideoTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));
  return RUN_ALL_TESTS();
}
