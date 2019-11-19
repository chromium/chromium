// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/audio_pipe_reader.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class AudioPipeReaderTest : public testing::Test,
                            public AudioPipeReader::StreamObserver {
 public:
  AudioPipeReaderTest()
    : stop_at_position_(-1) {
  }

  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    pipe_path_ = test_dir_.GetPath().AppendASCII("test_pipe");
    audio_thread_.reset(new base::Thread("TestAudioThread"));
    audio_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    reader_ = AudioPipeReader::Create(audio_thread_->task_runner(),
                                      pipe_path_);
    reader_->AddObserver(this);
  }

  // AudioPipeReader::StreamObserver interface.
  void OnDataRead(scoped_refptr<base::RefCountedString> data) override {
    read_data_ += data->data();
    if (stop_at_position_ > 0 &&
        static_cast<int>(read_data_.size()) >= stop_at_position_) {
      stop_at_position_ = -1;
      run_loop_->Quit();
    }
  }

  void CreatePipe() {
    ASSERT_EQ(0, mkfifo(pipe_path_.value().c_str(), 0600));
    output_.reset(new base::File(
        pipe_path_, base::File::FLAG_OPEN | base::File::FLAG_WRITE));
    ASSERT_TRUE(output_->IsValid());
  }

  void DeletePipe() {
    output_.reset();
    ASSERT_EQ(0, unlink(pipe_path_.value().c_str()));
  }

  void WaitForInput(int num_bytes) {
    run_loop_.reset(new base::RunLoop());
    stop_at_position_ = read_data_.size() + num_bytes;
    run_loop_->Run();
  }

  void WriteAndWait(const std::string& data) {
    ASSERT_EQ(static_cast<int>(data.size()),
              output_->WriteAtCurrentPos(data.data(), data.size()));
    WaitForInput(data.size());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<base::Thread> audio_thread_;
  base::ScopedTempDir test_dir_;
  base::FilePath pipe_path_;
  std::unique_ptr<base::File> output_;

  scoped_refptr<AudioPipeReader> reader_;

  std::string read_data_;
  int stop_at_position_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipeReaderTest);
};

// Verify that the reader can detect when the pipe is created and destroyed.
TEST_F(AudioPipeReaderTest, CreateAndDestroyPipe) {
  ASSERT_NO_FATAL_FAILURE(CreatePipe());
  ASSERT_NO_FATAL_FAILURE(WriteAndWait("ABCD"));
  ASSERT_NO_FATAL_FAILURE(DeletePipe());

  ASSERT_NO_FATAL_FAILURE(CreatePipe());
  ASSERT_NO_FATAL_FAILURE(WriteAndWait("abcd"));
  ASSERT_NO_FATAL_FAILURE(DeletePipe());

  EXPECT_EQ("ABCDabcd", read_data_);
}

// Verifies that the reader reads at the right speed.
TEST_F(AudioPipeReaderTest, Pacing) {
  int test_data_size = AudioPipeReader::kSamplingRate *
                       AudioPipeReader::kChannels *
                       AudioPipeReader::kBytesPerSample / 2;
  std::string test_data(test_data_size, '\0');

  ASSERT_NO_FATAL_FAILURE(CreatePipe());

  base::TimeTicks start_time = base::TimeTicks::Now();
  ASSERT_NO_FATAL_FAILURE(WriteAndWait(test_data));
  base::TimeDelta time_passed = base::TimeTicks::Now() - start_time;

  EXPECT_EQ(test_data, read_data_);
  EXPECT_GE(time_passed, base::TimeDelta::FromMilliseconds(500));
}

}  // namespace remoting
