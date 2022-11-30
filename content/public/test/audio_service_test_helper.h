// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_

#include <memory>

#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/mojom/testing_api.mojom.h"

namespace content {

// Used by testing environments to inject test-only interface support into an
// audio service instance. Test suites should create a long-lived instance of
// this class to ensure that instances of the Audio service in the process are
// able to fulfill test API binding requests.
class AudioServiceTestHelper {
 public:
  AudioServiceTestHelper();

  AudioServiceTestHelper(const AudioServiceTestHelper&) = delete;
  AudioServiceTestHelper& operator=(const AudioServiceTestHelper&) = delete;

  ~AudioServiceTestHelper();

 private:
  class TestingApi;

  void BindTestingApiReceiver(
      mojo::PendingReceiver<audio::mojom::TestingApi> receiver);

  std::unique_ptr<TestingApi> testing_api_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_
