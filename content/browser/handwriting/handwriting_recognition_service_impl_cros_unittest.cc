// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognition_service_impl_cros.h"

#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
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

// The recognition conversion code should have some ability to handle the wrong
// recognition output from OS (at least do not crash). Here we test the case
// that `ink_range->end_stroke >= strokes.size()` which can cause crash if this
// case is not checked.
TEST_F(HandwritingRecognitionServiceImplCrOSTest,
       GetPredictionInvalidRecognitionResult) {
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
  auto recognition_result =
      chromeos::machine_learning::mojom::HandwritingRecognizerResult::New();
  recognition_result->status = chromeos::machine_learning::mojom::
      HandwritingRecognizerResult::Status::OK;
  auto candidate =
      chromeos::machine_learning::mojom::HandwritingRecognizerCandidate::New();
  candidate->text = "text wrote";
  candidate->score = 0.4f;  // Does not matter because we do not use it.
  auto segmentation = chromeos::machine_learning::mojom::
      HandwritingRecognizerSegmentation::New();
  auto segment =
      chromeos::machine_learning::mojom::HandwritingRecognizerSegment::New();
  segment->sublabel = "seg";
  auto ink_range =
      chromeos::machine_learning::mojom::HandwritingRecognizerInkRange::New();
  ink_range->start_stroke = 0u;
  // ink_range->end_stroke is set to be larger than the number of input strokes.
  ink_range->end_stroke = 1u;
  ink_range->start_stroke = 10u;
  ink_range->end_stroke = 12u;
  segment->ink_ranges.emplace_back(std::move(ink_range));
  segmentation->segments.emplace_back(std::move(segment));
  candidate->segmentation = std::move(segmentation);
  recognition_result->candidates.emplace_back(std::move(candidate));
  GetMlServiceConnection().SetOutputHandwritingRecognizerResult(
      std::move(recognition_result));

  // Use 0 input strokes.
  is_callback_called = false;
  base::RunLoop runloop_prediction;
  recognizer_remote->GetPrediction(
      std::vector<handwriting::mojom::HandwritingStrokePtr>(),
      handwriting::mojom::HandwritingHints::New(),
      base::BindLambdaForTesting(
          [&](base::Optional<std::vector<
                  handwriting::mojom::HandwritingPredictionPtr>> result) {
            is_callback_called = true;
            // No result is returned because `ink_range->end_stroke >=
            // strokes.size()`.
            ASSERT_FALSE(result.has_value());
            runloop_prediction.Quit();
          }));
  runloop_prediction.Run();
  EXPECT_TRUE(is_callback_called);
}

// In this test we provide valid input/output to check the conversion code works
// correctly.
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
  auto recognition_result =
      chromeos::machine_learning::mojom::HandwritingRecognizerResult::New();
  recognition_result->status = chromeos::machine_learning::mojom::
      HandwritingRecognizerResult::Status::OK;
  auto candidate =
      chromeos::machine_learning::mojom::HandwritingRecognizerCandidate::New();
  candidate->text = "text wrote";
  candidate->score = 0.4f;  // Does not matter because we do not use it.
  auto segmentation = chromeos::machine_learning::mojom::
      HandwritingRecognizerSegmentation::New();
  auto segment =
      chromeos::machine_learning::mojom::HandwritingRecognizerSegment::New();
  segment->sublabel = "seg";
  auto ink_range =
      chromeos::machine_learning::mojom::HandwritingRecognizerInkRange::New();
  ink_range->start_stroke = 0u;
  ink_range->end_stroke = 1u;
  ink_range->start_point = 10u;
  ink_range->end_point = 12u;
  segment->ink_ranges.emplace_back(std::move(ink_range));
  segmentation->segments.emplace_back(std::move(segment));
  candidate->segmentation = std::move(segmentation);
  recognition_result->candidates.emplace_back(std::move(candidate));
  GetMlServiceConnection().SetOutputHandwritingRecognizerResult(
      std::move(recognition_result));

  // Generate 3 input strokes. Note that the number of strokes should be larger
  // than `ink_range->end_stroke+1`.
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
          [&](base::Optional<std::vector<
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
