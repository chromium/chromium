// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/reconfigurable_audio_bus_pool.h"

#include <memory>

#include "base/check_op.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
constexpr base::TimeDelta kAudioBusPoolDuration = base::Milliseconds(500);

// The test fixture.
class ReconfigurableAudioBusPoolTest : public ::testing::Test {
 public:
  ReconfigurableAudioBusPoolTest() = default;
  ReconfigurableAudioBusPoolTest(const ReconfigurableAudioBusPoolTest&) =
      delete;
  ReconfigurableAudioBusPoolTest& operator=(
      const ReconfigurableAudioBusPoolTest&) = delete;

  ~ReconfigurableAudioBusPoolTest() override = default;

 protected:
  std::unique_ptr<ReconfigurableAudioBusPoolImpl>
      reconfigurable_audio_bus_pool_;
};

TEST_F(ReconfigurableAudioBusPoolTest, Reconfigure) {
  AudioParameters original_parameters(AudioParameters::AUDIO_PCM_LINEAR,
                                      ChannelLayoutConfig::Stereo(), 1000, 100);
  reconfigurable_audio_bus_pool_ =
      std::make_unique<ReconfigurableAudioBusPoolImpl>(kAudioBusPoolDuration);
  reconfigurable_audio_bus_pool_->Reconfigure(original_parameters);

  std::unique_ptr<AudioBus> original_audio_bus =
      reconfigurable_audio_bus_pool_->GetAudioBus();
  EXPECT_EQ(original_audio_bus->channels(), original_parameters.channels());
  EXPECT_EQ(original_audio_bus->frames(),
            original_parameters.frames_per_buffer());

  AudioParameters new_parameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 ChannelLayoutConfig::Mono(), 2000, 200);
  reconfigurable_audio_bus_pool_->Reconfigure(new_parameters);
  std::unique_ptr<AudioBus> new_audio_bus =
      reconfigurable_audio_bus_pool_->GetAudioBus();
  EXPECT_EQ(new_audio_bus->channels(), new_parameters.channels());
  EXPECT_EQ(new_audio_bus->frames(), new_parameters.frames_per_buffer());

  AudioBus* new_audio_bus_ptr = new_audio_bus.get();
  reconfigurable_audio_bus_pool_->InsertAudioBus(std::move(new_audio_bus));
  reconfigurable_audio_bus_pool_->InsertAudioBus(std::move(original_audio_bus));

  // The original_audio_bus should be discarded from the pool because the pool
  // has been reconfigured with different parameters.
  std::unique_ptr<AudioBus> recycled_audio_bus =
      reconfigurable_audio_bus_pool_->GetAudioBus();
  EXPECT_EQ(new_audio_bus_ptr, recycled_audio_bus.get());
}

TEST_F(ReconfigurableAudioBusPoolTest, MaxCapacity) {
  // Allocating 500ms worth of buffers with a sample rate of 1000 and frames per
  // buffer of 250 should result in a max capacity of two buffers.
  AudioParameters audio_parameters(AudioParameters::AUDIO_PCM_LINEAR,
                                   ChannelLayoutConfig::Stereo(), 1000, 250);
  reconfigurable_audio_bus_pool_ =
      std::make_unique<ReconfigurableAudioBusPoolImpl>(kAudioBusPoolDuration);
  reconfigurable_audio_bus_pool_->Reconfigure(audio_parameters);

  std::unique_ptr<AudioBus> first_audio_bus =
      reconfigurable_audio_bus_pool_->GetAudioBus();
  std::unique_ptr<AudioBus> second_audio_bus =
      reconfigurable_audio_bus_pool_->GetAudioBus();
  std::unique_ptr<AudioBus> third_audio_bus =
      reconfigurable_audio_bus_pool_->GetAudioBus();

  // Insert the 3 audio buses back into the pool.
  AudioBus* first_audio_bus_ptr = first_audio_bus.get();
  reconfigurable_audio_bus_pool_->InsertAudioBus(std::move(first_audio_bus));
  AudioBus* second_audio_bus_ptr = second_audio_bus.get();
  reconfigurable_audio_bus_pool_->InsertAudioBus(std::move(second_audio_bus));
  reconfigurable_audio_bus_pool_->InsertAudioBus(std::move(third_audio_bus));

  EXPECT_EQ(second_audio_bus_ptr,
            reconfigurable_audio_bus_pool_->GetAudioBus().get());
  EXPECT_EQ(first_audio_bus_ptr,
            reconfigurable_audio_bus_pool_->GetAudioBus().get());
}

}  // namespace media
