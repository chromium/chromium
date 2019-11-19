// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/mojom/testing_api.mojom.h"
#include "services/service_manager/public/cpp/binder_map.h"

namespace content {

// Used by testing environments to inject test-only interface binders into an
// audio service instance. Test suites should create a long-lived instance of
// this class and call RegisterAudioBinders() on a BinderMap which will be used
// to fulfill interface requests within the audio service.
class AudioServiceTestHelper {
 public:
  AudioServiceTestHelper();
  ~AudioServiceTestHelper();

  // Registers the helper's interfaces on |binders|. Note that this object must
  // must outlive |binders|.
  void RegisterAudioBinders(service_manager::BinderMap* binders);

 private:
  class TestingApi;

  void BindTestingApiReceiver(
      mojo::PendingReceiver<audio::mojom::TestingApi> receiver);

  std::unique_ptr<TestingApi> testing_api_;

  DISALLOW_COPY_AND_ASSIGN(AudioServiceTestHelper);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_
