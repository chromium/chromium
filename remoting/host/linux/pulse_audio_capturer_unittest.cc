// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pulse_audio_capturer.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/proto/audio.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kAudioPipeEnvVar[] = "CHROME_REMOTE_DESKTOP_AUDIO_PIPE";
constexpr char kAudioPipeSwitchName[] = "audio-pipe-name";

}  // namespace

class PulseAudioCapturerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    audio_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  void TearDown() override {
    PulseAudioCapturer::InitializePipeReader(nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir test_dir_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
};

TEST_F(PulseAudioCapturerTest, NotSupportedWithoutEnvVarOrSwitch) {
  base::ScopedEnvironmentVariableOverride env_override(kAudioPipeEnvVar);
  base::test::ScopedCommandLine scoped_command_line;

  EXPECT_FALSE(PulseAudioCapturer::InitializePipeReader(audio_task_runner_));
  EXPECT_FALSE(PulseAudioCapturer::IsSupported());
  EXPECT_EQ(PulseAudioCapturer::Create(), nullptr);
}

TEST_F(PulseAudioCapturerTest, InitializesFromEnvVar) {
  base::FilePath pipe_path = test_dir_.GetPath().AppendASCII("env_pipe");
  base::ScopedEnvironmentVariableOverride env_override(kAudioPipeEnvVar,
                                                       pipe_path.value());
  base::test::ScopedCommandLine scoped_command_line;

  EXPECT_TRUE(PulseAudioCapturer::InitializePipeReader(audio_task_runner_));
  EXPECT_TRUE(PulseAudioCapturer::IsSupported());
  EXPECT_NE(PulseAudioCapturer::Create(), nullptr);
}

TEST_F(PulseAudioCapturerTest, FallsBackToLegacyCommandLineSwitch) {
  base::FilePath pipe_path = test_dir_.GetPath().AppendASCII("switch_pipe");
  base::ScopedEnvironmentVariableOverride env_override(kAudioPipeEnvVar);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      kAudioPipeSwitchName, pipe_path);

  EXPECT_TRUE(PulseAudioCapturer::InitializePipeReader(audio_task_runner_));
  EXPECT_TRUE(PulseAudioCapturer::IsSupported());

  std::unique_ptr<AudioCapturer> capturer = PulseAudioCapturer::Create();
  ASSERT_NE(capturer, nullptr);

  base::test::TestFuture<std::unique_ptr<AudioPacket>> packet_future;
  ASSERT_TRUE(capturer->Start(packet_future.GetRepeatingCallback()));

  ASSERT_EQ(mkfifo(pipe_path.value().c_str(), 0600), 0);
  base::File output(pipe_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(output.IsValid());

  std::string non_silent_samples(16, '\x01');
  ASSERT_TRUE(
      output.WriteAtCurrentPosAndCheck(base::as_byte_span(non_silent_samples)));

  std::unique_ptr<AudioPacket> packet = packet_future.Take();
  ASSERT_NE(packet, nullptr);
  ASSERT_EQ(packet->data_size(), 1);
  EXPECT_EQ(packet->data(0), non_silent_samples);
}

TEST_F(PulseAudioCapturerTest, EnvVarTakesPrecedenceOverCommandLineSwitch) {
  base::FilePath env_pipe_path = test_dir_.GetPath().AppendASCII("env_pipe");
  base::FilePath switch_pipe_path =
      test_dir_.GetPath().AppendASCII("switch_pipe");
  base::ScopedEnvironmentVariableOverride env_override(kAudioPipeEnvVar,
                                                       env_pipe_path.value());
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      kAudioPipeSwitchName, switch_pipe_path);

  EXPECT_TRUE(PulseAudioCapturer::InitializePipeReader(audio_task_runner_));
  std::unique_ptr<AudioCapturer> capturer = PulseAudioCapturer::Create();
  ASSERT_NE(capturer, nullptr);

  base::test::TestFuture<std::unique_ptr<AudioPacket>> packet_future;
  ASSERT_TRUE(capturer->Start(packet_future.GetRepeatingCallback()));

  ASSERT_EQ(mkfifo(env_pipe_path.value().c_str(), 0600), 0);
  base::File output(env_pipe_path,
                    base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(output.IsValid());

  std::string non_silent_samples(16, '\x02');
  ASSERT_TRUE(
      output.WriteAtCurrentPosAndCheck(base::as_byte_span(non_silent_samples)));

  std::unique_ptr<AudioPacket> packet = packet_future.Take();
  ASSERT_NE(packet, nullptr);
  ASSERT_EQ(packet->data_size(), 1);
  EXPECT_EQ(packet->data(0), non_silent_samples);
}

}  // namespace remoting
