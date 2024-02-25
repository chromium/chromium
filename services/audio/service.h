// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SERVICE_H_
#define SERVICES_AUDIO_SERVICE_H_

#include <memory>
#include <optional>

#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/audio/public/mojom/testing_api.mojom.h"
#include "services/audio/stream_factory.h"
#include "services/audio/testing_api_binder.h"

namespace base {
class DeferredSequencedTaskRunner;
class SystemMonitor;
}  // namespace base

namespace media {
class AecdumpRecordingManager;
class AudioDeviceListenerMac;
class AudioManager;
class AudioLogFactory;
}  // namespace media

namespace audio {
class DebugRecording;
class DeviceNotifier;
class LogFactoryManager;
class SystemInfo;

class Service final : public mojom::AudioService {
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

  // If |enable_remote_client_support| is true, the service will make available
  // a DeviceNotifier object that allows clients to/ subscribe to notifications
  // about device changes and a LogFactoryManager object that allows clients to
  // set a factory for audio logs.
  Service(std::unique_ptr<AudioManagerAccessor> audio_manager_accessor,
          bool enable_remote_client_support,
          mojo::PendingReceiver<mojom::AudioService> receiver);

  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

  ~Service() final;

  // Returns a DeferredSequencedTaskRunner to be used to run the audio service
  // when launched in the browser process.
  static base::DeferredSequencedTaskRunner* GetInProcessTaskRunner();

  // Allows tests to override how SystemInfo interface receivers are bound.
  // Used by FakeSystemInfo.
  static void SetSystemInfoBinderForTesting(SystemInfoBinder binder);

  // Allows tests to inject support for TestingApi binding, which is normally
  // unsupported by the service.
  static void SetTestingApiBinderForTesting(TestingApiBinder binder);

 private:
  // mojom::AudioService implementation:
  void BindSystemInfo(
      mojo::PendingReceiver<mojom::SystemInfo> receiver) override;
  void BindDebugRecording(
      mojo::PendingReceiver<mojom::DebugRecording> receiver) override;
  void BindStreamFactory(mojo::PendingReceiver<media::mojom::AudioStreamFactory>
                             receiver) override;
  void BindDeviceNotifier(
      mojo::PendingReceiver<mojom::DeviceNotifier> receiver) override;
  void BindLogFactoryManager(
      mojo::PendingReceiver<mojom::LogFactoryManager> receiver) override;
  void BindTestingApi(
      mojo::PendingReceiver<mojom::TestingApi> receiver) override;

  // Initializes a platform-specific device monitor for device-change
  // notifications. If the client uses the DeviceNotifier interface to get
  // notifications this function should be called before the DeviceMonitor is
  // created. If the client uses base::SystemMonitor to get notifications,
  // this function should be called on service startup.
  void InitializeDeviceMonitor();

  // The thread Service runs on should be the same as the main thread of
  // AudioManager provided by AudioManagerAccessor.
  THREAD_CHECKER(thread_checker_);

  base::RepeatingClosure quit_closure_;

  mojo::Receiver<mojom::AudioService> receiver_;
  std::unique_ptr<AudioManagerAccessor> audio_manager_accessor_;
  const bool enable_remote_client_support_;
  std::unique_ptr<base::SystemMonitor> system_monitor_;
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<media::AudioDeviceListenerMac> audio_device_listener_mac_;
#endif
  std::unique_ptr<SystemInfo> system_info_;

  // Manages starting / stopping of diagnostic audio processing recordings. Must
  // outlive |debug_recording_| and |stream_factory_|, if instantiated.
  std::unique_ptr<media::AecdumpRecordingManager> aecdump_recording_manager_;

  std::unique_ptr<DebugRecording> debug_recording_;
  std::optional<StreamFactory> stream_factory_;
  std::unique_ptr<DeviceNotifier> device_notifier_;
  std::unique_ptr<LogFactoryManager> log_factory_manager_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SERVICE_H_
