// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "media/webrtc/ml_model_handle.h"
#include "services/audio/ml_model_manager.h"
#include "services/audio/test/fake_ml_model_handles.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace audio {
namespace {

// Creates a valid TfLite model file with its file name in its description.
base::File CreateTfLiteFile(base::ScopedTempDir& temp_dir,
                            const std::string_view filename) {
  // Create a TFLite model buffer with the filename in its description string.
  flatbuffers::FlatBufferBuilder builder(1024);
  auto description_offset = builder.CreateString(filename);
  tflite::ModelBuilder model_builder(builder);
  model_builder.add_description(description_offset);
  tflite::FinishModelBuffer(builder, model_builder.Finish());

  // Write buffer to file.
  // SAFETY: These FlatBuffer APIs guarantee a valid pointer & size pair.
  base::span<const uint8_t> model_data = UNSAFE_BUFFERS(
      base::span<const uint8_t>(builder.GetBufferPointer(), builder.GetSize()));
  base::FilePath path = temp_dir.GetPath().AppendASCII(filename);
  if (!base::WriteFile(path, model_data)) {
    return base::File();
  }
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

// Same interface as CreateTfLiteFile, but fills the file with invalid content.
base::File CreateInvalidTfLiteFile(base::ScopedTempDir& temp_dir,
                                   const std::string_view filename) {
  base::FilePath path = temp_dir.GetPath().AppendASCII(filename);
  if (!base::WriteFile(path, "bogus model data")) {
    return base::File();
  }
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

// Value-parameterized test suite to test MlModelManagerImpl with both
// neural residual echo estimation and voice isolation denoiser models.
class MlModelManagerImplTest
    : public ::testing::TestWithParam<mojom::MlModelType> {
 public:
  MlModelManagerImplTest() = default;

  MlModelManagerImplTest(const MlModelManagerImplTest&) = delete;
  MlModelManagerImplTest& operator=(const MlModelManagerImplTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ml_model_manager_ = std::make_unique<MlModelManagerImpl>();
  }

  // Returns `false` if tasks time out.
  bool RunUntilTasksFinishOrTimeOut() {
    return base::test::RunUntil(
        [&]() { return !ml_model_manager_->HasPendingTasksForTesting(); });
  }

  mojom::MlModelType model_type() const { return GetParam(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MlModelManagerImpl> ml_model_manager_;
  base::ScopedTempDir temp_dir_;
};

TEST_P(MlModelManagerImplTest, SetGetReturnsModel) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // Happy base case: Setting a model returns that model.
  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);
  const tflite::FlatBufferModel& model = model_handle->Get();
  EXPECT_EQ(model.GetModel()->description()->str(), "model.tflite");
}

TEST_P(MlModelManagerImplTest, SetGetGetReturnsSameModel) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());
  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle1 = ml_model_manager_->GetModel(model_type());

  // Calling Get again returns the same model.
  auto model_handle2 = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle2);
  const tflite::FlatBufferModel& model = model_handle2->Get();
  EXPECT_EQ(model.GetModel()->description()->str(), "model.tflite");
}

TEST_P(MlModelManagerImplTest, SetSetGetReturnsSecondModel) {
  base::File model_file1 = CreateTfLiteFile(temp_dir_, "model1.tflite");
  ASSERT_TRUE(model_file1.IsValid());
  base::File model_file2 = CreateTfLiteFile(temp_dir_, "model2.tflite");
  ASSERT_TRUE(model_file2.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file1));
  ml_model_manager_->SetModel(model_type(), std::move(model_file2));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // The second model replaces the first model.
  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);
  const tflite::FlatBufferModel& model = model_handle->Get();
  EXPECT_EQ(model.GetModel()->description()->str(), "model2.tflite");
}

TEST_P(MlModelManagerImplTest, SetGetSetGetReturnsSecondModel) {
  base::File model_file1 = CreateTfLiteFile(temp_dir_, "model1.tflite");
  ASSERT_TRUE(model_file1.IsValid());
  base::File model_file2 = CreateTfLiteFile(temp_dir_, "model2.tflite");
  ASSERT_TRUE(model_file2.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file1));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle1 = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle1);
  const tflite::FlatBufferModel& model1 = model_handle1->Get();
  EXPECT_EQ(model1.GetModel()->description()->str(), "model1.tflite");

  ml_model_manager_->SetModel(model_type(), std::move(model_file2));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // The second model is served to new clients.
  auto model_handle2 = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle2);
  const tflite::FlatBufferModel& model2 = model_handle2->Get();
  EXPECT_EQ(model2.GetModel()->description()->str(), "model2.tflite");
}

TEST_P(MlModelManagerImplTest, StopSetGetReturnsModel) {
  ml_model_manager_->StopServingModel(model_type());
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);

  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // Model is loaded, since stop was called before a model was served.
  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);
  const tflite::FlatBufferModel& model = model_handle->Get();
  EXPECT_EQ(model.GetModel()->description()->str(), "model.tflite");
}

TEST_P(MlModelManagerImplTest, SetGetStopGetReturnsNull) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);
  const tflite::FlatBufferModel& model = model_handle->Get();
  EXPECT_NE(model.GetModel(), nullptr);

  ml_model_manager_->StopServingModel(model_type());
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // No models served after Stop.
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);

  // The old reference should still be valid. We cannot directly check this, but
  // the test should at least not crash when we use the reference here.
  EXPECT_EQ(model.GetModel()->description()->str(), "model.tflite");
}

TEST_P(MlModelManagerImplTest, SetGetStopSetGetReturnsSecondModel) {
  base::File model_file1 = CreateTfLiteFile(temp_dir_, "model1.tflite");
  ASSERT_TRUE(model_file1.IsValid());
  base::File model_file2 = CreateTfLiteFile(temp_dir_, "model2.tflite");
  ASSERT_TRUE(model_file2.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file1));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());
  EXPECT_NE(ml_model_manager_->GetModel(model_type()), nullptr);

  ml_model_manager_->StopServingModel(model_type());
  ml_model_manager_->SetModel(model_type(), std::move(model_file2));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // The second model, set after the stop signal, is served.
  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);
  const tflite::FlatBufferModel& model = model_handle->Get();
  EXPECT_EQ(model.GetModel()->description()->str(), "model2.tflite");
}

TEST_P(MlModelManagerImplTest, SetGetWithInvalidModelReturnsNull) {
  base::File model_file = CreateInvalidTfLiteFile(temp_dir_, "invalid.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // An invalid model is not served.
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);
}

// Verifies that when InvalidateModel is called with a handle matching the
// actively served model, the model is immediately cleared and GetModel returns
// null.
TEST_P(MlModelManagerImplTest, InvalidateModelMatchingHandleClearsModel) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);

  ml_model_manager_->InvalidateModel(model_type(), model_handle);
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);
}

// Verifies that calling InvalidateModel with a stale or mismatched model handle
// (e.g. from an earlier model version) does not clear the newer actively served
// model.
TEST_P(MlModelManagerImplTest, InvalidateModelMismatchedHandleDoesNotClear) {
  base::File model_file1 = CreateTfLiteFile(temp_dir_, "model1.tflite");
  ASSERT_TRUE(model_file1.IsValid());
  base::File model_file2 = CreateTfLiteFile(temp_dir_, "model2.tflite");
  ASSERT_TRUE(model_file2.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file1));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle1 = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle1);

  ml_model_manager_->SetModel(model_type(), std::move(model_file2));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle2 = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle2);
  ASSERT_NE(model_handle1, model_handle2);

  // Attempting to invalidate with the stale handle does nothing.
  ml_model_manager_->InvalidateModel(model_type(), model_handle1);
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), model_handle2);
}

// Verifies that passing a null handle to InvalidateModel is safely ignored and
// leaves the currently active model served.
TEST_P(MlModelManagerImplTest, InvalidateNullModelDoesNothing) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);

  ml_model_manager_->InvalidateModel(model_type(), nullptr);
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), model_handle);
}

// Verifies that invalidating an existing model while a subsequent SetModel()
// task is in flight on ThreadPool clears the existing model immediately without
// cancelling or disrupting the in-flight load of the newer model.
TEST_P(MlModelManagerImplTest,
       InvalidateModelDuringInFlightSetModelPreservesNewModel) {
  base::File model_file1 = CreateTfLiteFile(temp_dir_, "model1.tflite");
  ASSERT_TRUE(model_file1.IsValid());
  base::File model_file2 = CreateTfLiteFile(temp_dir_, "model2.tflite");
  ASSERT_TRUE(model_file2.IsValid());

  // Set and load model 1.
  ml_model_manager_->SetModel(model_type(), std::move(model_file1));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle1 = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle1);

  // Queue model 2, but do NOT wait for background file read tasks to finish.
  ml_model_manager_->SetModel(model_type(), std::move(model_file2));

  // Invalidate model 1 while model 2 is actively in-flight on ThreadPool.
  ml_model_manager_->InvalidateModel(model_type(), model_handle1);

  // Model 1 is immediately cleared.
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);

  // Allow model 2 loading to finish.
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // Model 2 was not cancelled and is now actively served.
  auto model_handle2 = ml_model_manager_->GetModel(model_type());
  EXPECT_TRUE(model_handle2);
  EXPECT_NE(model_handle1, model_handle2);
}

// Verifies that calling InvalidateModel for a model type that has never been
// set or loaded is a safe no-op.
TEST_P(MlModelManagerImplTest, InvalidateUnsetModelDoesNothing) {
  auto unmanaged_handle = base::MakeRefCounted<FakeMlModelHandle>();

  // Invalidation for an unset model should be a safe no-op.
  ml_model_manager_->InvalidateModel(model_type(), unmanaged_handle);
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);
}

// Verifies that duplicate InvalidateModel calls for the same failing model
// handle are idempotent and do not cause errors or redundant state transitions.
TEST_P(MlModelManagerImplTest, DoubleInvalidationIsIdempotent) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetModel(model_type(), std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  auto model_handle = ml_model_manager_->GetModel(model_type());
  ASSERT_TRUE(model_handle);

  ml_model_manager_->InvalidateModel(model_type(), model_handle);
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);

  // Second invalidation with the same handle should be a clean no-op.
  ml_model_manager_->InvalidateModel(model_type(), model_handle);
  EXPECT_EQ(ml_model_manager_->GetModel(model_type()), nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MlModelManagerImplTest,
    ::testing::Values(mojom::MlModelType::kResidualEchoEstimation,
                      mojom::MlModelType::kVoiceIsolationDenoiser));

}  // namespace
}  // namespace audio
