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
#include "services/audio/ml_model_manager.h"
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

// All tests here follow the same format:
// - A sequence of API calls:
//   - Set (SetResidualEchoEstimationModel)
//   - Get (GetResidualEchoEstimationModel)
//   - Stop (StopServingResidualEchoEstimationModel)
// - An expected result from the last Get call.
class MlModelManagerImplTest : public ::testing::Test {
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

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MlModelManagerImpl> ml_model_manager_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(MlModelManagerImplTest, SetGetReturnsModel) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetResidualEchoEstimationModel(std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // Happy base case: Setting a model returns that model.
  const tflite::FlatBufferModel* model =
      ml_model_manager_->GetResidualEchoEstimationModel();
  EXPECT_NE(model, nullptr);
  EXPECT_EQ(model->GetModel()->description()->str(), "model.tflite");
}

TEST_F(MlModelManagerImplTest, StopSetGetReturnsNull) {
  ml_model_manager_->StopServingResidualEchoEstimationModel();
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());
  EXPECT_EQ(ml_model_manager_->GetResidualEchoEstimationModel(), nullptr);

  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetResidualEchoEstimationModel(std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // Model is loaded, since stop was called before a model was served.
  const tflite::FlatBufferModel* model =
      ml_model_manager_->GetResidualEchoEstimationModel();
  EXPECT_NE(model, nullptr);
  EXPECT_EQ(model->GetModel()->description()->str(), "model.tflite");
}

TEST_F(MlModelManagerImplTest, SetGetStopGetReturnsNull) {
  base::File model_file = CreateTfLiteFile(temp_dir_, "model.tflite");
  ASSERT_TRUE(model_file.IsValid());

  ml_model_manager_->SetResidualEchoEstimationModel(std::move(model_file));
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  raw_ptr<const tflite::FlatBufferModel> first_model =
      ml_model_manager_->GetResidualEchoEstimationModel();
  EXPECT_NE(first_model, nullptr);
  ml_model_manager_->StopServingResidualEchoEstimationModel();
  ASSERT_TRUE(RunUntilTasksFinishOrTimeOut());

  // No models served after Stop.
  EXPECT_EQ(ml_model_manager_->GetResidualEchoEstimationModel(), nullptr);

  // The old pointer should still be served. We cannot directly check this, but
  // the test should at least not crash when we use the pointer here.
  EXPECT_EQ(first_model->GetModel()->description()->str(), "model.tflite");
}

}  // namespace
}  // namespace audio
