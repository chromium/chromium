// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_bus_pool.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// The test fixture.
class AudioBusPoolTest : public ::testing::Test {
 public:
  AudioBusPoolTest()
      : params_(AudioParameters::AUDIO_PCM_LINEAR,
                ChannelLayoutConfig::Stereo(),
                1000,
                100) {}

  AudioBusPoolTest(const AudioBusPoolTest&) = delete;
  AudioBusPoolTest& operator=(const AudioBusPoolTest&) = delete;

  ~AudioBusPoolTest() override = default;

  MOCK_METHOD0(OnCreateAudioBus, void());

  // Creates an AudioBusPoolImpl and makes OnCreateAudioBus track when it
  // creates AudioBuses.
  void CreateAndTrackAudioBusPool(size_t preallocated, size_t max_capacity) {
    audio_bus_pool_.reset(new AudioBusPoolImpl(
        params_, preallocated, max_capacity,
        base::BindLambdaForTesting([&](const AudioParameters& params) {
          EXPECT_TRUE(params.Equals(params_));
          OnCreateAudioBus();
          return AudioBus::Create(params);
        })));
  }

 protected:
  const AudioParameters params_;

  std::unique_ptr<AudioBusPool> audio_bus_pool_;
};

TEST_F(AudioBusPoolTest, PublicConstructor) {
  // The other tests use a specialized callback to create AudioBuses which also
  // tracks when it's called. We need to check that the default callback also
  // creates correct AudioBuses.
  audio_bus_pool_ = std::make_unique<AudioBusPoolImpl>(params_, 0, 10);
  std::unique_ptr<AudioBus> audio_bus = audio_bus_pool_->GetAudioBus();
  EXPECT_EQ(audio_bus->channels(), params_.channels());
  EXPECT_EQ(audio_bus->frames(), params_.frames_per_buffer());
}

TEST_F(AudioBusPoolTest, ReuseAudioBusesInLifoOrder) {
  CreateAndTrackAudioBusPool(0, 10);
  EXPECT_CALL(*this, OnCreateAudioBus()).Times(0);

  std::unique_ptr<AudioBus> audio_bus_1 = AudioBus::Create(params_);
  std::unique_ptr<AudioBus> audio_bus_2 = AudioBus::Create(params_);
  audio_bus_1->channel(0)[0] = 123;
  audio_bus_2->channel(0)[0] = 456;
  audio_bus_pool_->InsertAudioBus(std::move(audio_bus_1));
  audio_bus_pool_->InsertAudioBus(std::move(audio_bus_2));

  EXPECT_EQ(456, audio_bus_pool_->GetAudioBus()->channel(0)[0]);
  EXPECT_EQ(123, audio_bus_pool_->GetAudioBus()->channel(0)[0]);
}

TEST_F(AudioBusPoolTest, Preallocate) {
  // Expect to create 5 AudioBuses while preallocating.
  EXPECT_CALL(*this, OnCreateAudioBus()).Times(5);
  CreateAndTrackAudioBusPool(5, 10);

  // Expect not to have to create any new AudioBuses the first 5 times.
  EXPECT_CALL(*this, OnCreateAudioBus()).Times(0);
  for (int i = 0; i < 5; i++) {
    audio_bus_pool_->GetAudioBus();
  }

  // Now the pool has run out of AudioBuses and we expect it to create a new
  // one.
  EXPECT_CALL(*this, OnCreateAudioBus());
  audio_bus_pool_->GetAudioBus();
}

TEST_F(AudioBusPoolTest, MaxCapacity) {
  const int kMaxCapacity = 10;
  CreateAndTrackAudioBusPool(0, kMaxCapacity);

  // Insert 15 AudioBuses, but the max capacity is only 10.
  for (int i = 0; i < 15; i++) {
    audio_bus_pool_->InsertAudioBus(AudioBus::Create(params_));
  }

  // Expect not to have to create any new AudioBuses during the first 10 calls
  // to GetAudioBus.
  EXPECT_CALL(*this, OnCreateAudioBus()).Times(0);
  for (int i = 0; i < 10; i++) {
    audio_bus_pool_->GetAudioBus();
  }

  // Despite having called InsertAudioBus 15 times above, since the max capacity
  // is only 10, we should now have run out of stored AudioBuses. We therefore
  // expect that we will have to create a new AudioBus when we call GetAudioBus.
  EXPECT_CALL(*this, OnCreateAudioBus());
  audio_bus_pool_->GetAudioBus();
}

}  // namespace media
