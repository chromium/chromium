// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/token.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

ServiceProcessInfo MakeFakeAudioServiceProcessInfo(base::ProcessId pid) {
  ServiceProcessInfo fake_audio_process_info;
  fake_audio_process_info.pid = pid;
  fake_audio_process_info.service_interface_name =
      audio::mojom::AudioService::Name_;
  return fake_audio_process_info;
}

}  // namespace

struct AudioServiceListenerTest : public testing::Test {
  AudioServiceListenerTest() {
    // This test environment is not set up to support out-of-process services.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAudioServiceOutOfProcess});
    test_clock.SetNowTicks(base::TimeTicks::Now());
  }

  base::test::ScopedFeatureList feature_list_;
  BrowserTaskEnvironment task_environment_;
  base::SimpleTestTickClock test_clock;
};

TEST_F(AudioServiceListenerTest, OnInitWithAudioService_ProcessIdNotNull) {
  AudioServiceListener audio_service_listener;
  constexpr base::ProcessId pid(42);
  ServiceProcessInfo audio_process_info = MakeFakeAudioServiceProcessInfo(pid);
  audio_service_listener.Init({audio_process_info});
  EXPECT_EQ(pid, audio_service_listener.GetProcessId());
}

}  // namespace content
