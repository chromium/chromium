// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

typedef CanvasAsyncBlobCreator::IdleTaskStatus IdleTaskStatus;

class MockCanvasAsyncBlobCreator : public CanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreator(scoped_refptr<StaticBitmapImage> image,
                             ImageEncodingMimeType mime_type,
                             Document* document,
                             bool fail_encoder_initialization = false)
      : CanvasAsyncBlobCreator(
            image,
            CanvasAsyncBlobCreator::GetImageEncodeOptionsForMimeType(mime_type),
            kHTMLCanvasToBlobCallback,
            nullptr,
            base::TimeTicks(),
            document,
            nullptr) {
    if (fail_encoder_initialization)
      fail_encoder_initialization_for_test_ = true;
    enforce_idle_encoding_for_test_ = true;
  }

  CanvasAsyncBlobCreator::IdleTaskStatus GetIdleTaskStatus() {
    return idle_task_status_;
  }

  MOCK_METHOD0(SignalTaskSwitchInStartTimeoutEventForTesting, void());
  MOCK_METHOD0(SignalTaskSwitchInCompleteTimeoutEventForTesting, void());

 protected:
  void CreateBlobAndReturnResult() override {}
  void CreateNullAndReturnResult() override {}
  void SignalAlternativeCodePathFinishedForTesting() override;
  void PostDelayedTaskToCurrentThread(const base::Location&,
                                      base::OnceClosure,
                                      double delay_ms) override;
};

void MockCanvasAsyncBlobCreator::SignalAlternativeCodePathFinishedForTesting() {
  test::ExitRunLoop();
}

void MockCanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(
    const base::Location& location,
    base::OnceClosure task,
    double delay_ms) {
  DCHECK(IsMainThread());
  Thread::Current()->GetTaskRunner()->PostTask(location, std::move(task));
}

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStart
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutStart(scoped_refptr<StaticBitmapImage> image,
                                         Document* document)
      : MockCanvasAsyncBlobCreator(image, kMimeTypePng, document) {}

 protected:
  void ScheduleInitiateEncoding(double) override {
    // Deliberately make scheduleInitiateEncoding do nothing so that idle
    // task never starts
  }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutComplete
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutComplete(
      scoped_refptr<StaticBitmapImage> image,
      Document* document,
      bool fail_encoder_initialization = false)
      : MockCanvasAsyncBlobCreator(image,
                                   kMimeTypePng,
                                   document,
                                   fail_encoder_initialization) {}

 protected:
  void ScheduleInitiateEncoding(double quality) override {
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::Bind(&MockCanvasAsyncBlobCreatorWithoutComplete::InitiateEncoding,
                  WrapPersistent(this), quality, base::TimeTicks::Max()));
  }

  void IdleEncodeRows(base::TimeTicks deadline) override {
    // Deliberately make idleEncodeRows do nothing so that idle task never
    // completes
  }
};

//==============================================================================

class CanvasAsyncBlobCreatorTest : public PageTestBase {
 public:
  void PrepareMockCanvasAsyncBlobCreatorWithoutStart();
  void PrepareMockCanvasAsyncBlobCreatorWithoutComplete();
  void PrepareMockCanvasAsyncBlobCreatorFail();

 protected:
  CanvasAsyncBlobCreatorTest();
  MockCanvasAsyncBlobCreator* AsyncBlobCreator() {
    return async_blob_creator_.Get();
  }
  void TearDown() override;

 private:

  Persistent<MockCanvasAsyncBlobCreator> async_blob_creator_;
};

CanvasAsyncBlobCreatorTest::CanvasAsyncBlobCreatorTest() = default;

scoped_refptr<StaticBitmapImage> CreateTransparentImage(int width, int height) {
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(width, height);
  if (!surface)
    return nullptr;
  return StaticBitmapImage::Create(surface->makeImageSnapshot());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutStart() {
  async_blob_creator_ =
      MakeGarbageCollected<MockCanvasAsyncBlobCreatorWithoutStart>(
          CreateTransparentImage(20, 20), &GetDocument());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutComplete() {
  async_blob_creator_ =
      MakeGarbageCollected<MockCanvasAsyncBlobCreatorWithoutComplete>(
          CreateTransparentImage(20, 20), &GetDocument());
}

void CanvasAsyncBlobCreatorTest::PrepareMockCanvasAsyncBlobCreatorFail() {
  // We reuse the class MockCanvasAsyncBlobCreatorWithoutComplete because
  // this test case is expected to fail at initialization step before
  // completion.
  async_blob_creator_ =
      MakeGarbageCollected<MockCanvasAsyncBlobCreatorWithoutComplete>(
          CreateTransparentImage(20, 20), &GetDocument(), true);
}

void CanvasAsyncBlobCreatorTest::TearDown() {
  async_blob_creator_ = nullptr;
}

//==============================================================================

TEST_F(CanvasAsyncBlobCreatorTest,
       IdleTaskNotStartedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not started when the
  // StartTimeoutEvent is inspecting the idle task status.
  // The whole image encoding process (including initialization)  will then
  // become carried out in the alternative code path instead.
  PrepareMockCanvasAsyncBlobCreatorWithoutStart();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInStartTimeoutEventForTesting());

  AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  test::EnterRunLoop();

  testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest,
       IdleTaskNotCompletedWhenCompleteTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not completed when the
  // CompleteTimeoutEvent is inspecting the idle task status.
  // The remaining image encoding process (excluding initialization)  will
  // then become carried out in the alternative code path instead.
  PrepareMockCanvasAsyncBlobCreatorWithoutComplete();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInCompleteTimeoutEventForTesting());

  AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  test::EnterRunLoop();

  testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, IdleTaskFailedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not failed during when
  // either the StartTimeoutEvent or the CompleteTimeoutEvent is inspecting
  // the idle task status.
  PrepareMockCanvasAsyncBlobCreatorFail();

  AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  test::EnterRunLoop();

  EXPECT_EQ(IdleTaskStatus::kIdleTaskFailed,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

static sk_sp<SkImage> DrawAndReturnImage(
    const std::pair<sk_sp<SkColorSpace>, SkColorType>& color_space_param) {
  SkPaint transparentRed, transparentGreen, transparentBlue, transparentBlack;
  transparentRed.setARGB(128, 155, 27, 27);
  transparentGreen.setARGB(128, 27, 155, 27);
  transparentBlue.setARGB(128, 27, 27, 155);
  transparentBlack.setARGB(128, 27, 27, 27);

  SkImageInfo info = SkImageInfo::Make(2, 2, color_space_param.second,
                                       SkAlphaType::kPremul_SkAlphaType,
                                       color_space_param.first);
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), transparentRed);
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(1, 0, 1, 1),
                                 transparentGreen);
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(0, 1, 1, 1), transparentBlue);
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(1, 1, 1, 1),
                                 transparentBlack);
  return surface->makeImageSnapshot();
}

TEST_F(CanvasAsyncBlobCreatorTest, ColorManagedConvertToBlob) {
  std::list<std::pair<sk_sp<SkColorSpace>, SkColorType>> color_space_params;
  color_space_params.push_back(std::pair<sk_sp<SkColorSpace>, SkColorType>(
      SkColorSpace::MakeSRGB(), kN32_SkColorType));
  color_space_params.push_back(std::pair<sk_sp<SkColorSpace>, SkColorType>(
      SkColorSpace::MakeSRGBLinear(), kRGBA_F16_SkColorType));
  color_space_params.push_back(std::pair<sk_sp<SkColorSpace>, SkColorType>(
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear, SkNamedGamut::kDCIP3),
      kRGBA_F16_SkColorType));
  color_space_params.push_back(std::pair<sk_sp<SkColorSpace>, SkColorType>(
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear, SkNamedGamut::kRec2020),
      kRGBA_F16_SkColorType));
  color_space_params.push_back(std::pair<sk_sp<SkColorSpace>, SkColorType>(
      nullptr, kRGBA_F16_SkColorType));
  color_space_params.push_back(
      std::pair<sk_sp<SkColorSpace>, SkColorType>(nullptr, kN32_SkColorType));

  std::list<String> blob_mime_types = {"image/png", "image/webp", "image/jpeg"};
  std::list<String> blob_color_spaces = {kSRGBImageColorSpaceName,
                                         kDisplayP3ImageColorSpaceName,
                                         kRec2020ImageColorSpaceName};
  std::list<String> blob_pixel_formats = {
      kRGBA8ImagePixelFormatName, kRGBA16ImagePixelFormatName,
  };

  // Maximum differences are both observed locally with
  // kRGBA16ImagePixelFormatName, kSRGBImageColorSpaceName and nil input color
  // space
  const unsigned uint8_color_tolerance = 3;
  const float f16_color_tolerance = 0.015;

  for (auto color_space_param : color_space_params) {
    for (auto blob_mime_type : blob_mime_types) {
      for (auto blob_color_space : blob_color_spaces) {
        for (auto blob_pixel_format : blob_pixel_formats) {
          // Create the StaticBitmapImage in canvas_color_space
          sk_sp<SkImage> source_image = DrawAndReturnImage(color_space_param);
          scoped_refptr<StaticBitmapImage> source_bitmap_image =
              StaticBitmapImage::Create(source_image);

          // Prepare encoding options
          ImageEncodeOptions* options = ImageEncodeOptions::Create();
          options->setQuality(1);
          options->setType(blob_mime_type);
          options->setColorSpace(blob_color_space);
          options->setPixelFormat(blob_pixel_format);

          // Encode the image using CanvasAsyncBlobCreator
          auto* async_blob_creator =
              MakeGarbageCollected<CanvasAsyncBlobCreator>(
                  source_bitmap_image, options,
                  CanvasAsyncBlobCreator::ToBlobFunctionType::
                      kHTMLCanvasConvertToBlobPromise,
                  base::TimeTicks(), &GetDocument(), nullptr);
          ASSERT_TRUE(async_blob_creator->EncodeImageForConvertToBlobTest());

          sk_sp<SkData> sk_data = SkData::MakeWithCopy(
              async_blob_creator->GetEncodedImageForConvertToBlobTest().data(),
              async_blob_creator->GetEncodedImageForConvertToBlobTest().size());
          sk_sp<SkImage> decoded_img = SkImage::MakeFromEncoded(sk_data);

          sk_sp<SkColorSpace> expected_color_space =
              CanvasAsyncBlobCreator::BlobColorSpaceToSkColorSpace(
                  blob_color_space);
          SkColorType expected_color_type =
              (blob_pixel_format == kRGBA8ImagePixelFormatName)
                  ? kN32_SkColorType
                  : kRGBA_F16_SkColorType;
          scoped_refptr<StaticBitmapImage> ref_bitmap =
              source_bitmap_image->ConvertToColorSpace(expected_color_space,
                                                       expected_color_type);
          sk_sp<SkImage> ref_image =
              ref_bitmap->PaintImageForCurrentFrame().GetSkImage();

          // Jpeg does not support transparent images.
          bool compare_alpha = (blob_mime_type != "image/jpeg");
          ASSERT_TRUE(ColorCorrectionTestUtils::MatchSkImages(
              ref_image, decoded_img, uint8_color_tolerance,
              f16_color_tolerance, compare_alpha));
        }
      }
    }
  }
}
}
