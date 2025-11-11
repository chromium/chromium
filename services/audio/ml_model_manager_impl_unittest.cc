// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/ml_model_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
namespace {

class MlModelManagerImplTest : public ::testing::Test {
 public:
  MlModelManagerImplTest() = default;

  MlModelManagerImplTest(const MlModelManagerImplTest&) = delete;
  MlModelManagerImplTest& operator=(const MlModelManagerImplTest&) = delete;

 protected:
  void SetUp() override {
    ml_model_manager_.BindReceiver(
        remote_ml_model_manager_.BindNewPipeAndPassReceiver());
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    base::FilePath model_path = test_data_dir.AppendASCII("services")
                                    .AppendASCII("audio")
                                    .AppendASCII("test")
                                    .AppendASCII("data")
                                    .AppendASCII("noop.tflite");
    ASSERT_TRUE(base::PathExists(model_path));

    base::File model_file(model_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(model_file.IsValid());

    remote_ml_model_manager_->SetResidualEchoEstimationModel(
        std::move(model_file));
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::MlModelManager> remote_ml_model_manager_;
  MlModelManagerImpl ml_model_manager_;
};

TEST_F(MlModelManagerImplTest, SetAndGetFile) {
  // Wait for the asynchronous model loading to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return ml_model_manager_.GetResidualEchoEstimationModel() != nullptr;
  }));
  EXPECT_NE(ml_model_manager_.GetResidualEchoEstimationModel(), nullptr);
}

}  // namespace
}  // namespace audio
