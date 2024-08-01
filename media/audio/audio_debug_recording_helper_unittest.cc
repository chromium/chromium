// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/audio_debug_recording_helper.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_bus_pool.h"
#include "media/base/audio_sample_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace media {

namespace {

const base::FilePath::CharType kFileName[] =
    FILE_PATH_LITERAL("debug_recording.output.1.wav");

}  // namespace

// Mock class for the audio file writer that the helper wraps.
class MockAudioDebugFileWriter : public AudioDebugFileWriter {
 public:
  explicit MockAudioDebugFileWriter(const AudioParameters& params,
                                    base::File file)
      : AudioDebugFileWriter(params, std::move(file), nullptr),
        reference_data_(nullptr) {}

  MockAudioDebugFileWriter(const MockAudioDebugFileWriter&) = delete;
  MockAudioDebugFileWriter& operator=(const MockAudioDebugFileWriter&) = delete;

  MOCK_METHOD0(DestructorCalled, void());
  ~MockAudioDebugFileWriter() override { DestructorCalled(); }

  MOCK_METHOD1(DoWrite, void(const AudioBus&));
  void Write(const AudioBus& data) override {
    CHECK(reference_data_);
    EXPECT_EQ(reference_data_->channels(), data.channels());
    EXPECT_EQ(reference_data_->frames(), data.frames());
    for (int i = 0; i < data.channels(); ++i) {
      const float* data_ptr = data.channel(i);
      float* ref_data_ptr = reference_data_->channel(i);
      for (int j = 0; j < data.frames(); ++j, ++data_ptr, ++ref_data_ptr) {
        EXPECT_EQ(*ref_data_ptr, *data_ptr);
      }
    }
    DoWrite(data);
  }

  // Set reference data to compare against. Must be called before Write() is
  // called.
  void SetReferenceData(AudioBus* reference_data) {
    EXPECT_EQ(params_.channels(), reference_data->channels());
    EXPECT_EQ(params_.frames_per_buffer(), reference_data->frames());
    reference_data_ = reference_data;
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

 private:
  raw_ptr<AudioBus> reference_data_;
};

// Sub-class of the helper that overrides the CreateAudioDebugFileWriter
// function to create the above mock instead.
class AudioDebugRecordingHelperUnderTest : public AudioDebugRecordingHelper {
 public:
  using ChangedWriterCallback =
      base::RepeatingCallback<void(MockAudioDebugFileWriter*)>;

  AudioDebugRecordingHelperUnderTest(
      const AudioParameters& params,
      base::OnceClosure on_destruction_closure,
      ChangedWriterCallback changed_writer_callback)
      : AudioDebugRecordingHelper(params, std::move(on_destruction_closure)),
        changed_writer_callback_(std::move(changed_writer_callback)) {}

  AudioDebugRecordingHelperUnderTest(
      const AudioDebugRecordingHelperUnderTest&) = delete;
  AudioDebugRecordingHelperUnderTest& operator=(
      const AudioDebugRecordingHelperUnderTest&) = delete;

  ~AudioDebugRecordingHelperUnderTest() override = default;

 private:
  // Creates the mock writer.
  AudioDebugFileWriter::Ptr CreateAudioDebugFileWriter(
      const AudioParameters& params,
      base::File file) override {
    MockAudioDebugFileWriter* writer =
        new MockAudioDebugFileWriter(params, std::move(file));
    changed_writer_callback_.Run(writer);
    return AudioDebugFileWriter::Ptr(
        writer, base::OnTaskRunnerDeleter(writer->GetTaskRunner()));
  }

  void WillDestroyAudioDebugFileWriter() override {
    changed_writer_callback_.Run(nullptr);
  }

  ChangedWriterCallback changed_writer_callback_;
};

class AudioDebugRecordingHelperTest : public ::testing::Test {
 public:
  AudioDebugRecordingHelperTest() = default;

  AudioDebugRecordingHelperTest(const AudioDebugRecordingHelperTest&) = delete;
  AudioDebugRecordingHelperTest& operator=(
      const AudioDebugRecordingHelperTest&) = delete;

  ~AudioDebugRecordingHelperTest() override = default;

  // Helper function that creates a recording helper.
  std::unique_ptr<AudioDebugRecordingHelper> CreateRecordingHelper(
      const AudioParameters& params,
      base::OnceClosure on_destruction_closure) {
    return std::make_unique<AudioDebugRecordingHelperUnderTest>(
        params, std::move(on_destruction_closure),
        base::BindLambdaForTesting(
            [&](MockAudioDebugFileWriter* mock_audio_file_writer) {
              mock_audio_file_writer_ = mock_audio_file_writer;
            }));
  }

  MOCK_METHOD0(OnAudioDebugRecordingHelperDestruction, void());

  // Bound and passed to AudioDebugRecordingHelper::EnableDebugRecording as
  // AudioDebugRecordingHelper::CreateWavFileCallback.
  void CreateWavFile(AudioDebugRecordingStreamType stream_type,
                     uint32_t id,
                     base::OnceCallback<void(base::File)> reply_callback) {
    // Check that AudioDebugRecordingHelper::EnableDebugRecording calls
    // CreateWavFileCallback with expected stream type and id.
    EXPECT_EQ(stream_type_, stream_type);
    EXPECT_EQ(id_, id);
    base::FilePath path(base::CreateUniqueTempDirectoryScopedToTest().Append(
        base::FilePath(kFileName)));
    base::File debug_file(
        path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    // Run |reply_callback| with a valid file for expected
    // MockAudioDebugFileWriter::Start mocked call to happen.
    std::move(reply_callback).Run(std::move(debug_file));
    paths_.push_back(std::move(path));
  }

  void VerifyAndDeleteWavFiles(size_t expected_file_count) {
    DCHECK_EQ(expected_file_count, paths_.size());
    task_environment_.RunUntilIdle();
    for (base::FilePath& path : paths_) {
      ASSERT_TRUE(base::DeleteFile(path));
    }
    paths_.clear();
  }

 protected:
  const AudioDebugRecordingStreamType stream_type_ =
      AudioDebugRecordingStreamType::kInput;
  const uint32_t id_ = 1;

  // The test task environment.
  base::test::TaskEnvironment task_environment_;

  // Used for testing to access the file writer having to go through the
  // internal |file_writer_lock_|.
  raw_ptr<MockAudioDebugFileWriter> mock_audio_file_writer_;

  std::vector<base::FilePath> paths_;
};

// Creates a helper with an on destruction closure, and verifies that it's run.
TEST_F(AudioDebugRecordingHelperTest, TestDestructionClosure) {
  const AudioParameters params;
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(
          params, base::BindOnce(&AudioDebugRecordingHelperTest::
                                     OnAudioDebugRecordingHelperDestruction,
                                 base::Unretained(this)));

  EXPECT_CALL(*this, OnAudioDebugRecordingHelperDestruction());

  VerifyAndDeleteWavFiles(0);
}

// Verifies that disable can be called without being enabled.
TEST_F(AudioDebugRecordingHelperTest, OnlyDisable) {
  const AudioParameters params;
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(params, base::OnceClosure());

  recording_helper->DisableDebugRecording();

  VerifyAndDeleteWavFiles(0);
}

TEST_F(AudioDebugRecordingHelperTest, EnableDisable) {
  const AudioParameters params;
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(params, base::OnceClosure());

  recording_helper->EnableDebugRecording(
      stream_type_, id_,
      base::BindOnce(&AudioDebugRecordingHelperTest::CreateWavFile,
                     base::Unretained(this)));
  EXPECT_CALL(*mock_audio_file_writer_, DestructorCalled());
  recording_helper->DisableDebugRecording();

  recording_helper->EnableDebugRecording(
      stream_type_, id_,
      base::BindOnce(&AudioDebugRecordingHelperTest::CreateWavFile,
                     base::Unretained(this)));
  EXPECT_CALL(*mock_audio_file_writer_, DestructorCalled());
  recording_helper->DisableDebugRecording();

  VerifyAndDeleteWavFiles(2);
}

TEST_F(AudioDebugRecordingHelperTest, OnData) {
  // Only channel layout and frames per buffer is used in the file writer and
  // AudioBus, the other parameters are ignored.
  const int number_of_frames = 100;
  const AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                               ChannelLayoutConfig::Stereo(), 0,
                               number_of_frames);

  // Setup some data.
  const int number_of_samples = number_of_frames * params.channels();
  const float step = std::numeric_limits<int16_t>::max() / number_of_frames;
  auto source_data = base::HeapArray<float>::Uninit(number_of_samples);
  for (float i = 0; i < number_of_samples; ++i) {
    source_data[i] = i * step;
  }
  std::unique_ptr<AudioBus> audio_bus = AudioBus::Create(params);
  audio_bus->FromInterleaved<Float32SampleTypeTraits>(source_data.data(),
                                                      number_of_frames);

  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(params, base::OnceClosure());

  // Should not do anything.
  recording_helper->OnData(audio_bus.get());

  recording_helper->EnableDebugRecording(
      stream_type_, id_,
      base::BindOnce(&AudioDebugRecordingHelperTest::CreateWavFile,
                     base::Unretained(this)));
  mock_audio_file_writer_->SetReferenceData(audio_bus.get());

  EXPECT_CALL(*mock_audio_file_writer_, DoWrite(_));
  recording_helper->OnData(audio_bus.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_audio_file_writer_, DestructorCalled());
  recording_helper->DisableDebugRecording();

  // Make sure we clear the loop before enabling again.
  base::RunLoop().RunUntilIdle();

  // Enable again, this time with two OnData() calls, one OnData() call without
  // running the message loop until after disabling, and one call after
  // disabling.
  recording_helper->EnableDebugRecording(
      stream_type_, id_,
      base::BindOnce(&AudioDebugRecordingHelperTest::CreateWavFile,
                     base::Unretained(this)));
  mock_audio_file_writer_->SetReferenceData(audio_bus.get());

  EXPECT_CALL(*mock_audio_file_writer_, DoWrite(_)).Times(2);
  recording_helper->OnData(audio_bus.get());
  recording_helper->OnData(audio_bus.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_audio_file_writer_, DestructorCalled());
  recording_helper->DisableDebugRecording();

  // This call should not yield a DoWrite() call on the mock.
  recording_helper->OnData(audio_bus.get());
  base::RunLoop().RunUntilIdle();

  VerifyAndDeleteWavFiles(2);
}

}  // namespace media
