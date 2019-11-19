// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/audio_service_test_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

class AudioServiceTestHelper::TestingApi : public audio::mojom::TestingApi {
 public:
  TestingApi() = default;
  ~TestingApi() override = default;

  // audio::mojom::TestingApi implementation
  void Crash() override {
    LOG(ERROR) << "Intentionally crashing audio service for testing.";
    // Use |TerminateCurrentProcessImmediately()| instead of |CHECK()| to avoid
    // 'Fatal error' dialog on Windows debug.
    base::Process::TerminateCurrentProcessImmediately(1);
  }

  void BindReceiver(mojo::PendingReceiver<audio::mojom::TestingApi> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<audio::mojom::TestingApi> receivers_;

  DISALLOW_COPY_AND_ASSIGN(TestingApi);
};

AudioServiceTestHelper::AudioServiceTestHelper()
    : testing_api_(new TestingApi) {}

AudioServiceTestHelper::~AudioServiceTestHelper() = default;

void AudioServiceTestHelper::RegisterAudioBinders(
    service_manager::BinderMap* binders) {
  if (!base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess))
    return;

  binders->Add(base::BindRepeating(
      &AudioServiceTestHelper::BindTestingApiReceiver, base::Unretained(this)));
}

void AudioServiceTestHelper::BindTestingApiReceiver(
    mojo::PendingReceiver<audio::mojom::TestingApi> receiver) {
  testing_api_->BindReceiver(std::move(receiver));
}

}  // namespace content
