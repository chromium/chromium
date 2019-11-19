// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_sync_reader.h"

#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/sync_socket.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;

namespace media {

void NoLog(const std::string&) {}

static_assert(
    std::is_unsigned<
        decltype(AudioOutputBufferParameters::bitstream_data_size)>::value,
    "If |bitstream_data_size| is ever made signed, add tests for negative "
    "buffer sizes.");

enum OverflowTestCase {
  kZero,
  kNoOverflow,
  kOverflowByOne,
  kOverflowByOneThousand,
  kOverflowByMax
};

static const OverflowTestCase overflow_test_case_values[]{
    kZero, kNoOverflow, kOverflowByOne, kOverflowByOneThousand, kOverflowByMax};

class AudioSyncReaderBitstreamTest : public TestWithParam<OverflowTestCase> {
 public:
  AudioSyncReaderBitstreamTest() {}
  ~AudioSyncReaderBitstreamTest() override {}

 private:
  base::test::TaskEnvironment env_;
};

TEST_P(AudioSyncReaderBitstreamTest, BitstreamBufferOverflow_DoesNotWriteOOB) {
  const int kSampleRate = 44100;
  const int kFramesPerBuffer = 1;
  AudioParameters params(AudioParameters::AUDIO_BITSTREAM_AC3,
                         CHANNEL_LAYOUT_STEREO, kSampleRate, kFramesPerBuffer);

  auto socket = std::make_unique<base::CancelableSyncSocket>();
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params);
  std::unique_ptr<AudioSyncReader> reader = AudioSyncReader::Create(
      base::BindRepeating(&NoLog), params, socket.get());
  const base::WritableSharedMemoryMapping shmem =
      reader->TakeSharedMemoryRegion().Map();
  AudioOutputBuffer* buffer = static_cast<AudioOutputBuffer*>(shmem.memory());
  reader->RequestMoreData(base::TimeDelta(), base::TimeTicks(), 0);

  uint32_t signal;
  EXPECT_EQ(socket->Receive(&signal, sizeof(signal)), sizeof(signal));

  // So far, this is an ordinary stream.
  // Now |reader| expects data to be writted to the shared memory. The renderer
  // says how much data was written.
  switch (GetParam()) {
    case kZero:
      buffer->params.bitstream_data_size = 0;
      break;
    case kNoOverflow:
      buffer->params.bitstream_data_size =
          shmem.mapped_size() - sizeof(AudioOutputBufferParameters);
      break;
    case kOverflowByOne:
      buffer->params.bitstream_data_size =
          shmem.mapped_size() - sizeof(AudioOutputBufferParameters) + 1;
      break;
    case kOverflowByOneThousand:
      buffer->params.bitstream_data_size =
          shmem.mapped_size() - sizeof(AudioOutputBufferParameters) + 1000;
      break;
    case kOverflowByMax:
      buffer->params.bitstream_data_size = std::numeric_limits<decltype(
          buffer->params.bitstream_data_size)>::max();
      break;
  }

  ++signal;
  EXPECT_EQ(socket->Send(&signal, sizeof(signal)), sizeof(signal));

  // The purpose of the test is to ensure this call doesn't result in undefined
  // behavior, which should be verified by sanitizers.
  reader->Read(output_bus.get());
}

INSTANTIATE_TEST_SUITE_P(AudioSyncReaderTest,
                         AudioSyncReaderBitstreamTest,
                         ::testing::ValuesIn(overflow_test_case_values));

}  // namespace media
