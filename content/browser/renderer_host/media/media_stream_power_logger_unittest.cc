// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_power_logger.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::testing::Contains;
using ::testing::Each;
using ::testing::HasSubstr;
using ::testing::Not;

constexpr int kRenderProcessId = 1;

class MediaStreamPowerLoggerTest : public ::testing::Test {
 public:
  MediaStreamPowerLoggerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::make_unique<MockVideoCaptureProvider>());

    MediaStreamManager::RegisterNativeLogCallback(
        kRenderProcessId,
        base::BindRepeating(&MediaStreamPowerLoggerTest::OnLogMessage,
                            base::Unretained(this)));
  }

  ~MediaStreamPowerLoggerTest() override {
    MediaStreamManager::UnregisterNativeLogCallback(kRenderProcessId);
    audio_manager_->Shutdown();
  }

 protected:
  void OnLogMessage(const std::string& message) {
    messages_.push_back(message);
  }

  std::vector<std::string> messages_;
  std::unique_ptr<media::MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  // `media_stream_manager_` must outlive `task_environment_` because it is a
  // CurrentThread::DestructionObserver.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  BrowserTaskEnvironment task_environment_;
};

TEST_F(MediaStreamPowerLoggerTest, LogMessagesDoNotContainPointers) {
  MediaStreamPowerLogger logger;
  logger.OnSuspend();
  logger.OnResume();
  logger.OnThermalStateChange(
      base::PowerThermalObserver::DeviceThermalState::kNominal);
  logger.OnSpeedLimitChange(50);
  ASSERT_TRUE(base::test::RunUntil([&]() { return messages_.size() == 4; }));

  EXPECT_THAT(messages_, Contains(HasSubstr("MSPL::OnSuspend([id=")));
  EXPECT_THAT(messages_, Contains(HasSubstr("MSPL::OnResume([id=")));
  EXPECT_THAT(messages_,
              Contains(HasSubstr("MSPL::OnThermalStateChange({id=")));
  EXPECT_THAT(messages_, Contains(HasSubstr("MSPL::OnSpeedLimitChange({id=")));
  EXPECT_THAT(messages_, Each(Not(HasSubstr("0x"))));
}

}  // namespace
}  // namespace content
