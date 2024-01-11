// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognition_service_impl_cros.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/ml_switches.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "content/browser/handwriting/handwriting_recognizer_impl_cros.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace content {

class HandwritingRecognitionServiceImplCrOSTest
    : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_ml_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
    // We need to add the switch to "enable" HWR support.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kOndeviceHandwritingSwitch, "use_rootfs");
  }

  chromeos::machine_learning::FakeServiceConnectionImpl&
  GetMlServiceConnection() {
    return fake_ml_service_connection_;
  }

 private:
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_ml_service_connection_;
};

TEST_F(HandwritingRecognitionServiceImplCrOSTest, CreateHandwritingRecognizer) {
  mojo::Remote<handwriting::mojom::HandwritingRecognitionService>
      service_remote;
  CrOSHandwritingRecognitionServiceImpl::Create(
      service_remote.BindNewPipeAndPassReceiver());
  auto model_constraint = handwriting::mojom::HandwritingModelConstraint::New();
  model_constraint->languages.push_back("en");
  bool is_callback_called = false;
  base::RunLoop runloop;
  service_remote->CreateHandwritingRecognizer(
      std::move(model_constraint),
      base::BindLambdaForTesting(
          [&](handwriting::mojom::CreateHandwritingRecognizerResult result,
              mojo::PendingRemote<handwriting::mojom::HandwritingRecognizer>
                  remote) {
            EXPECT_EQ(
                result,
                handwriting::mojom::CreateHandwritingRecognizerResult::kOk);
            is_callback_called = true;
            runloop.Quit();
          }));
  runloop.Run();
  EXPECT_TRUE(is_callback_called);
}

// In this test we provide valid input/output to check the mojo calls and data
// copying code work correctly.
TEST_F(HandwritingRecognitionServiceImplCrOSTest,
       GetPredictionCorrectConversion) {
  mojo::Remote<handwriting::mojom::HandwritingRecognitionService>
      service_remote;
  CrOSHandwritingRecognitionServiceImpl::Create(
      service_remote.BindNewPipeAndPassReceiver());
  auto model_constraint = handwriting::mojom::HandwritingModelConstraint::New();
  model_constraint->languages.push_back("en");
  bool is_callback_called = false;
  mojo::Remote<handwriting::mojom::HandwritingRecognizer> recognizer_remote;
  base::RunLoop runloop_create_recognizer;
  service_remote->CreateHandwritingRecognizer(
      std::move(model_constraint),
      base::BindLambdaForTesting(
          [&](handwriting::mojom::CreateHandwritingRecognizerResult result,
              mojo::PendingRemote<handwriting::mojom::HandwritingRecognizer>
                  input_remote) {
            is_callback_called = true;
            ASSERT_EQ(
                result,
                handwriting::mojom::CreateHandwritingRecognizerResult::kOk);
            recognizer_remote.Bind(std::move(input_remote));
            runloop_create_recognizer.Quit();
          }));
  runloop_create_recognizer.Run();
  ASSERT_TRUE(is_callback_called);

  // Generate and set the fake recognition result.
  auto prediction = chromeos::machine_learning::web_platform::mojom::
      HandwritingPrediction::New();
  prediction->text = "text wrote";
  auto segment = chromeos::machine_learning::web_platform::mojom::
      HandwritingSegment::New();
  segment->grapheme = "seg";
  segment->begin_index = 0u;
  segment->end_index = 3u;
  segment->drawing_segments.push_back(
      chromeos::machine_learning::web_platform::mojom::
          HandwritingDrawingSegment::New(0u, 10u, 15u));
  segment->drawing_segments.push_back(
      chromeos::machine_learning::web_platform::mojom::
          HandwritingDrawingSegment::New(1u, 0u, 13u));
  prediction->segmentation_result.push_back(std::move(segment));

  std::vector<
      chromeos::machine_learning::web_platform::mojom::HandwritingPredictionPtr>
      predictions;
  predictions.push_back(std::move(prediction));
  GetMlServiceConnection().SetOutputWebPlatformHandwritingRecognizerResult(
      std::move(predictions));

  // Generate 3 input strokes.
  std::vector<handwriting::mojom::HandwritingStrokePtr> strokes;
  const std::vector<int> num_points = {15, 10, 21};
  for (int npts : num_points) {
    auto stroke = handwriting::mojom::HandwritingStroke::New();
    for (int i = 0; i < npts; ++i) {
      // The actual values of the points do not matter.
      stroke->points.emplace_back(handwriting::mojom::HandwritingPoint::New());
    }
    strokes.emplace_back(std::move(stroke));
  }

  is_callback_called = false;
  base::RunLoop runloop_prediction;
  recognizer_remote->GetPrediction(
      std::move(strokes), handwriting::mojom::HandwritingHints::New(),
      base::BindLambdaForTesting(
          [&](std::optional<std::vector<
                  handwriting::mojom::HandwritingPredictionPtr>> result) {
            is_callback_called = true;
            ASSERT_TRUE(result.has_value());
            ASSERT_EQ(result.value().size(), 1u);
            EXPECT_EQ(result.value()[0]->text, "text wrote");
            ASSERT_EQ(result.value()[0]->segmentation_result.size(), 1u);
            EXPECT_EQ(result.value()[0]->segmentation_result[0]->grapheme,
                      "seg");
            // It is 0 because it is the first segment.
            EXPECT_EQ(result.value()[0]->segmentation_result[0]->begin_index,
                      0u);
            // This is the Length of the grapheme "seg".
            EXPECT_EQ(result.value()[0]->segmentation_result[0]->end_index, 3u);
            // Equals `ink_range->end_stroke-ink_range->begin_stroke+1`.
            ASSERT_EQ(result.value()[0]
                          ->segmentation_result[0]
                          ->drawing_segments.size(),
                      2u);
            EXPECT_EQ(result.value()[0]
                          ->segmentation_result[0]
                          ->drawing_segments[0]
                          ->stroke_index,
                      0u);
            // Equals `ink_range->start_point`.
            EXPECT_EQ(result.value()[0]
                          ->segmentation_result[0]
                          ->drawing_segments[0]
                          ->begin_point_index,
                      10u);
            // Equals `num_points[0]`.
            EXPECT_EQ(result.value()[0]
                          ->segmentation_result[0]
                          ->drawing_segments[0]
                          ->end_point_index,
                      15u);
            EXPECT_EQ(result.value()[0]
                          ->segmentation_result[0]
                          ->drawing_segments[1]
                          ->stroke_index,
                      1u);
            EXPECT_EQ(result.value()[0]
                          ->segmentation_result[0]
                          ->drawing_segments[1]
                          ->begin_point_index,
                      0u);
            // Equals `ink_range->end_point+1`.
            EXPECT_EQ(result.value()[0]
                          ->segmentation_result[0]
                          ->drawing_segments[1]
                          ->end_point_index,
                      13u);
            runloop_prediction.Quit();
          }));
  runloop_prediction.Run();
  EXPECT_TRUE(is_callback_called);
}

}  // namespace content
