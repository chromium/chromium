// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/audio_service_test_helper.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/audio/service.h"

namespace content {

class AudioServiceTestHelper::TestingApi : public audio::mojom::TestingApi {
 public:
  TestingApi() = default;

  TestingApi(const TestingApi&) = delete;
  TestingApi& operator=(const TestingApi&) = delete;

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
};

AudioServiceTestHelper::AudioServiceTestHelper()
    : testing_api_(std::make_unique<TestingApi>()) {
  audio::Service::SetTestingApiBinderForTesting(base::BindRepeating(
      &AudioServiceTestHelper::BindTestingApiReceiver, base::Unretained(this)));
}

AudioServiceTestHelper::~AudioServiceTestHelper() {
  audio::Service::SetTestingApiBinderForTesting(base::NullCallback());
}

void AudioServiceTestHelper::BindTestingApiReceiver(
    mojo::PendingReceiver<audio::mojom::TestingApi> receiver) {
  CHECK(base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess))
      << "Audio Service API binder shouldn't be invoked from this process when "
         "the AudioServiceOutOfProcess feature is disabled.";
  testing_api_->BindReceiver(std::move(receiver));
}

}  // namespace content
