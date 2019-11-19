// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/stream_factory.h"

#include <memory>

#include "base/test/task_environment.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/audio/traced_service_ref.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

// Stream creation is tested as part of the stream unit tests.
TEST(AudioServiceStreamFactoryTest, TakesServiceRef) {
  base::test::TaskEnvironment env;
  service_manager::ServiceKeepalive keepalive{nullptr, base::nullopt};
  media::MockAudioManager audio_manager(
      std::make_unique<media::TestAudioThread>());

  StreamFactory factory(&audio_manager);

  mojo::Remote<mojom::StreamFactory> remote_factory;

  factory.Bind(
      remote_factory.BindNewPipeAndPassReceiver(),
      TracedServiceRef(keepalive.CreateRef(), "audio::StreamFactory binding"));
  EXPECT_FALSE(keepalive.HasNoRefs());
  remote_factory.reset();
  env.RunUntilIdle();
  EXPECT_TRUE(keepalive.HasNoRefs());
  audio_manager.Shutdown();
}

}  // namespace audio
