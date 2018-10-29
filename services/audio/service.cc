// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/system/system_monitor.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_manager.h"
#include "services/audio/debug_recording.h"
#include "services/audio/device_notifier.h"
#include "services/audio/log_factory_manager.h"
#include "services/audio/service_metrics.h"
#include "services/audio/system_info.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

#if defined(OS_MACOSX)
#include "media/audio/mac/audio_device_listener_mac.h"
#endif

namespace audio {

Service::Service(std::unique_ptr<AudioManagerAccessor> audio_manager_accessor,
                 base::TimeDelta quit_timeout,
                 bool enable_remote_client_support,
                 std::unique_ptr<service_manager::BinderRegistry> registry)
    : quit_timeout_(quit_timeout),
      audio_manager_accessor_(std::move(audio_manager_accessor)),
      enable_remote_client_support_(enable_remote_client_support),
      registry_(std::move(registry)) {
  DCHECK(audio_manager_accessor_);
  if (enable_remote_client_support_) {
    log_factory_manager_ = std::make_unique<LogFactoryManager>();
    audio_manager_accessor_->SetAudioLogFactory(
        log_factory_manager_->GetLogFactory());
  } else {
    // Start device monitoring explicitly if no mojo device notifier will be
    // created. This is required for in-process device notifications.
    InitializeDeviceMonitor();
  }
}

Service::~Service() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("audio", "audio::Service::~Service");

  metrics_.reset();

  // |ref_factory_| may reentrantly call its |quit_closure| when we reset the
  // members below. Destroy it ahead of time to prevent this.
  ref_factory_.reset();

  // Stop all streams cleanly before shutting down the audio manager.
  stream_factory_.reset();

  // Reset |debug_recording_| to disable debug recording before AudioManager
  // shutdown.
  debug_recording_.reset();

  audio_manager_accessor_->Shutdown();
}

void Service::OnStart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("audio", "audio::Service::OnStart")

  // This will pre-create AudioManager if AudioManagerAccessor owns it.
  CHECK(audio_manager_accessor_->GetAudioManager());

  metrics_ =
      std::make_unique<ServiceMetrics>(base::DefaultTickClock::GetInstance());
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      base::BindRepeating(&Service::MaybeRequestQuitDelayed,
                          base::Unretained(this)));
  registry_->AddInterface<mojom::SystemInfo>(base::BindRepeating(
      &Service::BindSystemInfoRequest, base::Unretained(this)));
  registry_->AddInterface<mojom::DebugRecording>(base::BindRepeating(
      &Service::BindDebugRecordingRequest, base::Unretained(this)));
  registry_->AddInterface<mojom::StreamFactory>(base::BindRepeating(
      &Service::BindStreamFactoryRequest, base::Unretained(this)));
  if (enable_remote_client_support_) {
    registry_->AddInterface<mojom::DeviceNotifier>(base::BindRepeating(
        &Service::BindDeviceNotifierRequest, base::Unretained(this)));
    registry_->AddInterface<mojom::LogFactoryManager>(base::BindRepeating(
        &Service::BindLogFactoryManagerRequest, base::Unretained(this)));
  }
}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK(ref_factory_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT1("audio", "audio::Service::OnBindInterface", "interface",
               interface_name);

  if (ref_factory_->HasNoRefs())
    metrics_->HasConnections();

  registry_->BindInterface(interface_name, std::move(interface_pipe));
  DCHECK(!ref_factory_->HasNoRefs());
  quit_timer_.AbandonAndStop();
}

bool Service::OnServiceManagerConnectionLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return true;
}

void Service::SetQuitClosureForTesting(base::RepeatingClosure quit_closure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  quit_closure_ = std::move(quit_closure);
}

void Service::BindSystemInfoRequest(mojom::SystemInfoRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);

  if (!system_info_) {
    system_info_ = std::make_unique<SystemInfo>(
        audio_manager_accessor_->GetAudioManager());
  }
  system_info_->Bind(
      std::move(request),
      TracedServiceRef(ref_factory_->CreateRef(), "audio::SystemInfo Binding"));
}

void Service::BindDebugRecordingRequest(mojom::DebugRecordingRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);
  TracedServiceRef service_ref(ref_factory_->CreateRef(),
                               "audio::DebugRecording Binding");

  // Accept only one bind request at a time. Old request is overwritten.
  // |debug_recording_| must be reset first to disable debug recording, and then
  // create a new DebugRecording instance to enable debug recording.
  debug_recording_.reset();
  debug_recording_ = std::make_unique<DebugRecording>(
      std::move(request), audio_manager_accessor_->GetAudioManager(),
      std::move(service_ref));
}

void Service::BindStreamFactoryRequest(mojom::StreamFactoryRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);

  if (!stream_factory_)
    stream_factory_.emplace(audio_manager_accessor_->GetAudioManager());
  stream_factory_->Bind(std::move(request),
                        TracedServiceRef(ref_factory_->CreateRef(),
                                         "audio::StreamFactory Binding"));
}

void Service::BindDeviceNotifierRequest(mojom::DeviceNotifierRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);
  DCHECK(enable_remote_client_support_);

  if (!system_monitor_) {
    CHECK(!base::SystemMonitor::Get());
    system_monitor_ = std::make_unique<base::SystemMonitor>();
  }
  InitializeDeviceMonitor();
  if (!device_notifier_)
    device_notifier_ = std::make_unique<DeviceNotifier>();
  device_notifier_->Bind(std::move(request),
                         TracedServiceRef(ref_factory_->CreateRef(),
                                          "audio::DeviceNotifier Binding"));
}

void Service::BindLogFactoryManagerRequest(
    mojom::LogFactoryManagerRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);
  DCHECK(log_factory_manager_);
  DCHECK(enable_remote_client_support_);
  log_factory_manager_->Bind(
      std::move(request), TracedServiceRef(ref_factory_->CreateRef(),
                                           "audio::LogFactoryManager Binding"));
}

void Service::MaybeRequestQuitDelayed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metrics_->HasNoConnections();
  if (quit_timeout_ <= base::TimeDelta())
    return;
  quit_timer_.Start(FROM_HERE, quit_timeout_, this, &Service::MaybeRequestQuit);
}

void Service::MaybeRequestQuit() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_ && ref_factory_->HasNoRefs() &&
         quit_timeout_ > base::TimeDelta());
  TRACE_EVENT0("audio", "audio::Service::MaybeRequestQuit");

  context()->CreateQuitClosure().Run();
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void Service::InitializeDeviceMonitor() {
#if defined(OS_MACOSX)
  if (audio_device_listener_mac_)
    return;

  TRACE_EVENT0("audio", "audio::Service::InitializeDeviceMonitor");

  audio_device_listener_mac_ = std::make_unique<media::AudioDeviceListenerMac>(
      base::BindRepeating([] {
        if (base::SystemMonitor::Get()) {
          base::SystemMonitor::Get()->ProcessDevicesChanged(
              base::SystemMonitor::DEVTYPE_AUDIO);
        }
      }),
      true /* monitor_default_input */, true /* monitor_addition_removal */,
      true /* monitor_sources */);
#endif
}

}  // namespace audio
