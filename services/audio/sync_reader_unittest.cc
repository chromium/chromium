// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/sync_reader.h"

#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;

using media::AudioBus;
using media::AudioOutputBuffer;
using media::AudioOutputBufferParameters;
using media::AudioParameters;

namespace audio {

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

class SyncReaderBitstreamTest : public TestWithParam<OverflowTestCase> {
 public:
  SyncReaderBitstreamTest() {}
  ~SyncReaderBitstreamTest() override {}

 private:
  base::test::TaskEnvironment env_;
};

TEST_P(SyncReaderBitstreamTest, BitstreamBufferOverflow_DoesNotWriteOOB) {
  const int kSampleRate = 44100;
  const int kFramesPerBuffer = 1;
  AudioParameters params(AudioParameters::AUDIO_BITSTREAM_AC3,
                         media::ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFramesPerBuffer);

  auto socket = std::make_unique<base::CancelableSyncSocket>();
  SyncReader reader(base::BindRepeating(&NoLog), params, socket.get());
  ASSERT_TRUE(reader.IsValid());
  const base::WritableSharedMemoryMapping shmem =
      reader.TakeSharedMemoryRegion().Map();
  ASSERT_TRUE(shmem.IsValid());
  auto* const buffer =
      reinterpret_cast<media::AudioOutputBuffer*>(shmem.memory());
  ASSERT_TRUE(buffer);
  reader.RequestMoreData(base::TimeDelta(), base::TimeTicks(), 0);

  uint32_t signal;
  EXPECT_EQ(socket->Receive(&signal, sizeof(signal)), sizeof(signal));

  // So far, this is an ordinary stream.
  // Now |reader| expects data to be written to the shared memory. The renderer
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
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params);
  reader.Read(output_bus.get(), false);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncReaderBitstreamTest,
                         ::testing::ValuesIn(overflow_test_case_values));

}  // namespace audio
