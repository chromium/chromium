// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/process/process.h"
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

ServiceProcessInfo MakeFakeAudioServiceProcessInfo() {
  ServiceProcessInfo fake_audio_process_info(
      audio::mojom::AudioService::Name_, /*site=*/std::nullopt,
      content::ServiceProcessId(), base::Process::Current());
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
  ServiceProcessInfo audio_process_info = MakeFakeAudioServiceProcessInfo();
  std::vector<ServiceProcessInfo> info;
  info.push_back(std::move(audio_process_info));
  audio_service_listener.Init(std::move(info));
  EXPECT_EQ(base::Process::Current().Pid(),
            audio_service_listener.GetProcess().Pid());
}

}  // namespace content
