// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_

#include "base/gtest_prod_util.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/service_process_info.h"

namespace content {

// Tracks the system's active audio service instance, if any exists.
class CONTENT_EXPORT AudioServiceListener : public AudioServiceProcessObserver {
 public:
  AudioServiceListener();

  AudioServiceListener(const AudioServiceListener&) = delete;
  AudioServiceListener& operator=(const AudioServiceListener&) = delete;

  ~AudioServiceListener() override;

  base::Process GetProcess() const;

  // Clears cached process state and rebinds the sequence checker.
  // Called by ResetAudioServiceForTesting().
  void ResetForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithoutAudioService_ProcessIdNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithAudioService_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnAudioServiceCreated_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           StartService_LogStartStatus);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnServiceTerminatedNormally);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest, OnServiceCrashed);

  void OnServiceLaunched(const ServiceProcessInfo& info) override;
  void OnServiceTerminatedNormally(const ServiceProcessInfo& info) override;
  void OnServiceCrashed(const ServiceProcessInfo& info) override;

  void MaybeSetLogFactory();

  base::Process audio_process_;
  bool log_factory_is_set_ = false;
  SEQUENCE_CHECKER(owning_sequence_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
