// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/system/system_monitor.h"
#include "base/task/bind_post_task.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/aecdump_recording_manager.h"
#include "media/audio/audio_manager.h"
#include "media/media_buildflags.h"
#include "services/audio/debug_recording.h"
#include "services/audio/device_notifier.h"
#include "services/audio/log_factory_manager.h"
#include "services/audio/system_info.h"

#if BUILDFLAG(IS_MAC)
#include "media/audio/mac/audio_device_listener_mac.h"
#endif

namespace audio {

Service::Service(std::unique_ptr<AudioManagerAccessor> audio_manager_accessor,
                 bool enable_remote_client_support,
                 mojo::PendingReceiver<mojom::AudioService> receiver)
    : receiver_(this, std::move(receiver)),
      audio_manager_accessor_(std::move(audio_manager_accessor)),
      enable_remote_client_support_(enable_remote_client_support) {
  DCHECK(audio_manager_accessor_);

  if (enable_remote_client_support_) {
    CHECK(!base::SystemMonitor::Get());
    system_monitor_ = std::make_unique<base::SystemMonitor>();
    log_factory_manager_ = std::make_unique<LogFactoryManager>();
    audio_manager_accessor_->SetAudioLogFactory(
        log_factory_manager_->GetLogFactory());
  } else {
    // Start device monitoring explicitly if no mojo device notifier will be
    // created. This is required for in-process device notifications.
    InitializeDeviceMonitor();
  }
  TRACE_EVENT0("audio", "audio::Service::OnStart");

  // This will pre-create AudioManager if AudioManagerAccessor owns it.
  CHECK(audio_manager_accessor_->GetAudioManager());

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION) || BUILDFLAG(IS_CHROMEOS)
  aecdump_recording_manager_ = std::make_unique<media::AecdumpRecordingManager>(
      audio_manager_accessor_->GetAudioManager()->GetTaskRunner());

  // This is no-op except on ChromeOS.
  audio_manager_accessor_->GetAudioManager()->SetAecDumpRecordingManager(
      aecdump_recording_manager_->AsWeakPtr());
#endif
}

Service::~Service() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("audio", "audio::Service::~Service");

  // Stop all streams cleanly before shutting down the audio manager.
  stream_factory_.reset();

  // Reset |debug_recording_| to disable debug recording before AudioManager
  // shutdown.
  debug_recording_.reset();

  audio_manager_accessor_->Shutdown();
}

// static
base::DeferredSequencedTaskRunner* Service::GetInProcessTaskRunner() {
  static const base::NoDestructor<
      scoped_refptr<base::DeferredSequencedTaskRunner>>
      instance(base::MakeRefCounted<base::DeferredSequencedTaskRunner>());
  return instance->get();
}

// static
void Service::SetSystemInfoBinderForTesting(SystemInfoBinder binder) {
  GetSystemInfoBinderForTesting() = std::move(binder);
}

// static
void Service::SetTestingApiBinderForTesting(TestingApiBinder binder) {
  GetTestingApiBinder() = std::move(binder);
}

void Service::BindSystemInfo(
    mojo::PendingReceiver<mojom::SystemInfo> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto& binder_override = GetSystemInfoBinderForTesting();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  if (!system_info_) {
    system_info_ = std::make_unique<SystemInfo>(
        audio_manager_accessor_->GetAudioManager());
  }
  system_info_->Bind(std::move(receiver));
}

void Service::BindDebugRecording(
    mojo::PendingReceiver<mojom::DebugRecording> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Accept only one bind request at a time. Old receiver is overwritten.
  // |debug_recording_| must be reset first to disable debug recording, and then
  // create a new DebugRecording instance to enable debug recording.
  debug_recording_.reset();
  debug_recording_ = std::make_unique<DebugRecording>(
      std::move(receiver), audio_manager_accessor_->GetAudioManager(),
      aecdump_recording_manager_.get());
}

void Service::BindStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!stream_factory_) {
    stream_factory_.emplace(audio_manager_accessor_->GetAudioManager(),
                            aecdump_recording_manager_.get());
  }
  stream_factory_->Bind(std::move(receiver));
}

void Service::BindDeviceNotifier(
    mojo::PendingReceiver<mojom::DeviceNotifier> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(enable_remote_client_support_);

  InitializeDeviceMonitor();
  if (!device_notifier_)
    device_notifier_ = std::make_unique<DeviceNotifier>();
  device_notifier_->Bind(std::move(receiver));
}

void Service::BindLogFactoryManager(
    mojo::PendingReceiver<mojom::LogFactoryManager> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(log_factory_manager_);
  DCHECK(enable_remote_client_support_);
  log_factory_manager_->Bind(std::move(receiver));
}

void Service::BindTestingApi(
    mojo::PendingReceiver<mojom::TestingApi> receiver) {
  auto& binder = GetTestingApiBinder();
  if (binder)
    binder.Run(std::move(receiver));
}

void Service::InitializeDeviceMonitor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_MAC)
  if (audio_device_listener_mac_)
    return;

  TRACE_EVENT0("audio", "audio::Service::InitializeDeviceMonitor");

  audio_device_listener_mac_ = media::AudioDeviceListenerMac::Create(
      base::BindPostTaskToCurrentDefault(base::BindRepeating([] {
        if (auto* monitor = base::SystemMonitor::Get())
          monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
      })),
      /*monitor_sample_rate_changes=*/false,
      /*monitor_default_input=*/true,
      /*monitor_addition_removal=*/true,
      /*monitor_sources=*/true);
#endif
}

}  // namespace audio
