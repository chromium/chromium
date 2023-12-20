// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/log_factory_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

using testing::_;
using testing::SaveArg;

namespace {

class MockAudioLog : public media::mojom::AudioLog {
 public:
  MockAudioLog() {}
  MOCK_METHOD2(OnCreated,
               void(const media::AudioParameters& params,
                    const std::string& device_id));

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnSetVolume, void(double));
  MOCK_METHOD1(OnProcessingStateChanged, void(const std::string&));
  MOCK_METHOD1(OnLogMessage, void(const std::string&));
};

class MockAudioLogFactory : public media::mojom::AudioLogFactory {
 public:
  MockAudioLogFactory(
      mojo::PendingReceiver<media::mojom::AudioLogFactory> receiver,
      size_t num_mock_logs)
      : receiver_(this, std::move(receiver)) {
    for (size_t i = 0; i < num_mock_logs; ++i)
      mock_logs_.push_back(new MockAudioLog());
  }

  MockAudioLogFactory(const MockAudioLogFactory&) = delete;
  MockAudioLogFactory& operator=(const MockAudioLogFactory&) = delete;

  MOCK_METHOD2(MockCreateAudioLog,
               void(media::mojom::AudioLogComponent, int32_t));

  void CreateAudioLog(media::mojom::AudioLogComponent component,
                      int32_t component_id,
                      mojo::PendingReceiver<media::mojom::AudioLog>
                          audio_log_receiver) override {
    MockCreateAudioLog(component, component_id);
    mojo::MakeSelfOwnedReceiver(
        base::WrapUnique(mock_logs_[current_mock_log_++].get()),
        std::move(audio_log_receiver));
  }

  MockAudioLog* GetMockLog(size_t index) { return mock_logs_[index]; }

 private:
  mojo::Receiver<media::mojom::AudioLogFactory> receiver_;
  size_t current_mock_log_ = 0;
  std::vector<raw_ptr<MockAudioLog, VectorExperimental>> mock_logs_;
};

}  // namespace

class LogFactoryManagerTest : public ::testing::Test {
 public:
  LogFactoryManagerTest() = default;

  LogFactoryManagerTest(const LogFactoryManagerTest&) = delete;
  LogFactoryManagerTest& operator=(const LogFactoryManagerTest&) = delete;

 protected:
  void CreateLogFactoryManager() {
    log_factory_manager_ = std::make_unique<LogFactoryManager>();
    log_factory_manager_->Bind(
        remote_log_factory_manager_.BindNewPipeAndPassReceiver());
  }

  void DestroyLogFactoryManager() {
    remote_log_factory_manager_.reset();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::LogFactoryManager> remote_log_factory_manager_;
  std::unique_ptr<LogFactoryManager> log_factory_manager_;
};

TEST_F(LogFactoryManagerTest, LogFactoryManagerQueuesRequestsAndSetsFactory) {
  CreateLogFactoryManager();

  // Create a log before setting the log factory.
  const int kComponentId1 = 1;
  const double kVolume1 = 0.5;
  media::AudioLogFactory* log_factory = log_factory_manager_->GetLogFactory();
  std::unique_ptr<media::AudioLog> log1 = log_factory->CreateAudioLog(
      media::AudioLogFactory::AudioComponent::kAudioOutputStream,
      kComponentId1);
  log1->OnStarted();
  log1->OnSetVolume(kVolume1);
  log1->OnStopped();
  log1->OnClosed();

  // Set the factory.
  mojo::PendingRemote<media::mojom::AudioLogFactory> remote_log_factory;
  MockAudioLogFactory mock_factory(
      remote_log_factory.InitWithNewPipeAndPassReceiver(), 2);
  MockAudioLog* mock_log1 = mock_factory.GetMockLog(0);
  testing::InSequence s;

  // Set the factory and expect that queued operations run.
  EXPECT_CALL(mock_factory,
              MockCreateAudioLog(media::mojom::AudioLogComponent::kOutputStream,
                                 kComponentId1));
  EXPECT_CALL(*mock_log1, OnStarted());
  EXPECT_CALL(*mock_log1, OnSetVolume(kVolume1));
  EXPECT_CALL(*mock_log1, OnStopped());
  EXPECT_CALL(*mock_log1, OnClosed());
  remote_log_factory_manager_->SetLogFactory(std::move(remote_log_factory));
  task_environment_.RunUntilIdle();

  // Create another log after the factory is already set.
  const int kComponentId2 = 2;
  const double kVolume2 = 0.1;
  EXPECT_CALL(
      mock_factory,
      MockCreateAudioLog(media::mojom::AudioLogComponent::kInputController,
                         kComponentId2));
  MockAudioLog* mock_log2 = mock_factory.GetMockLog(1);
  EXPECT_CALL(*mock_log2, OnStarted());
  EXPECT_CALL(*mock_log2, OnSetVolume(kVolume2));
  EXPECT_CALL(*mock_log2, OnStopped());
  EXPECT_CALL(*mock_log2, OnClosed());

  std::unique_ptr<media::AudioLog> log2 = log_factory->CreateAudioLog(
      media::AudioLogFactory::AudioComponent::kAudioInputController, 2);
  log2->OnStarted();
  log2->OnSetVolume(kVolume2);
  log2->OnStopped();
  log2->OnClosed();

  // Ensure all mock objects are released.
  log1.reset();
  log2.reset();
  DestroyLogFactoryManager();
}

}  // namespace audio
