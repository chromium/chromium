// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SERVICE_H_
#define SERVICES_AUDIO_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/audio/stream_factory.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class DeferredSequencedTaskRunner;
class SystemMonitor;
}

namespace media {
class AudioDeviceListenerMac;
class AudioManager;
class AudioLogFactory;
}  // namespace media

namespace audio {
class DebugRecording;
class DeviceNotifier;
class LogFactoryManager;
class ServiceMetrics;
class SystemInfo;

class Service : public service_manager::Service {
 public:
  // Abstracts AudioManager ownership. Lives and must be accessed on a thread
  // its created on, and that thread must be AudioManager main thread.
  class AudioManagerAccessor {
   public:
    virtual ~AudioManagerAccessor() {}

    // Must be called before destruction to cleanly shut down AudioManager.
    // Service must ensure AudioManager is not called after that.
    virtual void Shutdown() = 0;

    // Returns a pointer to AudioManager.
    virtual media::AudioManager* GetAudioManager() = 0;

    // Attempts to associate |factory| with the audio manager.
    // |factory| must outlive the audio manager.
    // It only makes sense to call this method before GetAudioManager().
    virtual void SetAudioLogFactory(media::AudioLogFactory* factory) = 0;
  };

  // Service will attempt to quit if there are no connections to it within
  // |quit_timeout| interval. If |quit_timeout| is null the
  // service never quits. If |enable_remote_client_support| is true, the service
  // will make available a DeviceNotifier object that allows clients to
  // subscribe to notifications about device changes and a LogFactoryManager
  // object that allows clients to set a factory for audio logs.
  Service(std::unique_ptr<AudioManagerAccessor> audio_manager_accessor,
          base::Optional<base::TimeDelta> quit_timeout,
          bool enable_remote_client_support,
          std::unique_ptr<service_manager::BinderMap> extra_binders,
          mojo::PendingReceiver<service_manager::mojom::Service> receiver);
  ~Service() final;

  // Returns a DeferredSequencedTaskRunner to be used to run the audio service
  // when launched in the browser process.
  static base::DeferredSequencedTaskRunner* GetInProcessTaskRunner();

  // service_manager::Service implementation.
  void OnStart() final;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle receiver_pipe) final;
  void OnDisconnected() final;

 private:
  void BindSystemInfoReceiver(
      mojo::PendingReceiver<mojom::SystemInfo> receiver);
  void BindDebugRecordingReceiver(
      mojo::PendingReceiver<mojom::DebugRecording> receiver);
  void BindStreamFactoryReceiver(
      mojo::PendingReceiver<mojom::StreamFactory> receiver);
  void BindDeviceNotifierReceiver(
      mojo::PendingReceiver<mojom::DeviceNotifier> receiver);
  void BindLogFactoryManagerReceiver(
      mojo::PendingReceiver<mojom::LogFactoryManager> receiver);

  // Initializes a platform-specific device monitor for device-change
  // notifications. If the client uses the DeviceNotifier interface to get
  // notifications this function should be called before the DeviceMonitor is
  // created. If the client uses base::SystemMonitor to get notifications,
  // this function should be called on service startup.
  void InitializeDeviceMonitor();

  // The thread Service runs on should be the same as the main thread of
  // AudioManager provided by AudioManagerAccessor.
  THREAD_CHECKER(thread_checker_);

  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive keepalive_;

  base::RepeatingClosure quit_closure_;

  std::unique_ptr<AudioManagerAccessor> audio_manager_accessor_;
  const bool enable_remote_client_support_;
  std::unique_ptr<base::SystemMonitor> system_monitor_;
#if defined(OS_MACOSX)
  std::unique_ptr<media::AudioDeviceListenerMac> audio_device_listener_mac_;
#endif
  std::unique_ptr<SystemInfo> system_info_;
  std::unique_ptr<DebugRecording> debug_recording_;
  base::Optional<StreamFactory> stream_factory_;
  std::unique_ptr<DeviceNotifier> device_notifier_;
  std::unique_ptr<LogFactoryManager> log_factory_manager_;
  std::unique_ptr<ServiceMetrics> metrics_;

  std::unique_ptr<service_manager::BinderMap> binders_;

  // TODO(crbug.com/888478): Remove this after diagnosis.
  volatile uint32_t magic_bytes_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SERVICE_H_
