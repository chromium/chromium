// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/process/process.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

// Tracks the system's active audio service instance, if any exists.
class CONTENT_EXPORT AudioServiceListener
    : public ServiceProcessHost::Observer {
 public:
  AudioServiceListener();

  AudioServiceListener(const AudioServiceListener&) = delete;
  AudioServiceListener& operator=(const AudioServiceListener&) = delete;

  ~AudioServiceListener() override;

  base::Process GetProcess() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithoutAudioService_ProcessIdNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithAudioService_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnAudioServiceCreated_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           StartService_LogStartStatus);

  // Called by the constructor, or by tests to inject fake process info.
  void Init(std::vector<ServiceProcessInfo> running_service_processes);

  // ServiceProcessHost::Observer implementation:
  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override;
  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override;
  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override;

  void MaybeSetLogFactory();

  base::Process audio_process_;
  bool log_factory_is_set_ = false;
  SEQUENCE_CHECKER(owning_sequence_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
