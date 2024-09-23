// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/pulse_loopback_manager.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "media/audio/pulse/audio_manager_pulse.h"
#include "media/audio/pulse/pulse_input.h"
#include "media/audio/pulse/pulse_util.h"

namespace media {

// Special PulseAudio identifier.
constexpr char kDefaultSinkName[] = "@DEFAULT_SINK@";

// static
std::unique_ptr<PulseLoopbackManager> PulseLoopbackManager::Create(
    ReleaseStreamCallback release_stream_callback,
    pa_context* context,
    pa_threaded_mainloop* mainloop) {
  // We want to open the monitor device of the default sink, which is
  // specified using a special identifier.
  const std::string default_monitor_name =
      pulse::GetMonitorSourceNameForSink(mainloop, context, kDefaultSinkName);
  if (default_monitor_name.empty()) {
    LOG(ERROR) << "Failed to retrieve default monitor name.";
    return nullptr;
  }

  auto manager = base::WrapUnique(new PulseLoopbackManager(
      default_monitor_name, release_stream_callback, context, mainloop));

  // Register a callback to be notified about default sink changes. When such
  // an event occurs, we want to switch the source to the monitor of the new
  // default sink.
  pulse::AutoPulseLock auto_lock(manager->mainloop_);
  pa_context_set_subscribe_callback(manager->context_, &EventCallback,
                                    manager.get());

  // TODO(crbug.com/40281249): Check if subscription was reported as successful
  // in pulse::ContextSuccessCallback.
  pa_operation* operation =
      pa_context_subscribe(manager->context_, PA_SUBSCRIPTION_MASK_SERVER,
                           &pulse::ContextSuccessCallback, manager->mainloop_);
  if (!pulse::WaitForOperationCompletion(manager->mainloop_, operation)) {
    LOG(ERROR) << "Failed to subscribe to PulseAudio events.";
    return nullptr;
  }

  return manager;
}

PulseLoopbackManager::~PulseLoopbackManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  pulse::AutoPulseLock auto_lock(mainloop_);
  pa_operation* operation =
      pa_context_subscribe(context_, PA_SUBSCRIPTION_MASK_NULL,
                           &pulse::ContextSuccessCallback, mainloop_);
  if (!pulse::WaitForOperationCompletion(mainloop_, operation)) {
    LOG(ERROR) << "Failed to unsubscribe from PulseAudio events.";
  }
  pa_context_set_subscribe_callback(context_, nullptr, nullptr);
}

PulseLoopbackAudioStream* PulseLoopbackManager::MakeLoopbackStream(
    const AudioParameters& params,
    AudioManager::LogCallback log_callback,
    bool should_mute_system_audio) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Check if we're trying to create a loopback-with-muting when other loopbacks
  // are in progress
  // TODO(crbug.com/359102845): Support simultaneous loopback-with-muting
  // streams
  if (should_mute_system_audio && !streams_.empty()) {
    LOG(ERROR) << "Cannot create loopback-with-muting: other loopbacks are in "
                  "progress.";
    return nullptr;
  }

  // Check if we're trying to create a loopback-without-muting when a
  // loopback-with-muting exists
  if (!should_mute_system_audio && has_muting_loopback_) {
    LOG(ERROR) << "Cannot create loopback-without-muting: a "
                  "loopback-with-muting is in progress.";
    return nullptr;
  }

  auto* stream = new PulseLoopbackAudioStream(
      release_stream_callback_, GetLoopbackSourceName(), params, mainloop_,
      context_, std::move(log_callback), should_mute_system_audio);

  streams_.emplace_back(stream);

  if (should_mute_system_audio) {
    has_muting_loopback_ = true;
    pulse::MuteAllSinksExcept(mainloop_, context_, GetLoopbackSourceName());
  }

  return stream;
}

void PulseLoopbackManager::RemoveStream(AudioInputStream* stream) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::erase(streams_, reinterpret_cast<PulseLoopbackAudioStream*>(stream));
}

const std::string& PulseLoopbackManager::GetLoopbackSourceName() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return default_monitor_name_;
}

// static
void PulseLoopbackManager::EventCallback(pa_context* context,
                                         pa_subscription_event_type_t type,
                                         uint32_t index,
                                         void* user_data) {
  // It is safe to access `manager`, as the registration of the callback is
  // managed by the manager in the constructor and destructor.
  auto* manager = reinterpret_cast<PulseLoopbackManager*>(user_data);
  if ((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) ==
          PA_SUBSCRIPTION_EVENT_SERVER &&
      (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
          PA_SUBSCRIPTION_EVENT_CHANGE) {
    // This event can occur when the default sink or source is changed.
    // Post to the thread that created the PulseAudioInputStreams.
    manager->task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PulseLoopbackManager::OnServerChangeEvent,
                                  manager->weak_this_));
  }

  pa_threaded_mainloop_signal(manager->mainloop_, 0);
}

PulseLoopbackManager::PulseLoopbackManager(
    const std::string& default_monitor_name,
    ReleaseStreamCallback release_stream_callback,
    pa_context* context,
    pa_threaded_mainloop* mainloop)
    : default_monitor_name_(default_monitor_name),
      release_stream_callback_(release_stream_callback),
      context_(context),
      mainloop_(mainloop),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}

void PulseLoopbackManager::OnServerChangeEvent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::string default_monitor_name =
      pulse::GetMonitorSourceNameForSink(mainloop_, context_, kDefaultSinkName);
  if (default_monitor_name.empty()) {
    LOG(ERROR) << "Default sink does not have a monitor.";
    return;
  }

  // Verify that the event relates to a default sink change.
  if (default_monitor_name == default_monitor_name_) {
    return;
  }
  if (has_muting_loopback_) {
    pulse::UnmuteAllSinks(mainloop_, context_);
  }

  // There is no data race here, as OnServerChangeEvent() and
  // GetLoopbackSourceName() must run on the same `task_runner_`.
  default_monitor_name_ = default_monitor_name;

  for (PulseLoopbackAudioStream* stream : streams_) {
    stream->ChangeStreamSource(default_monitor_name_);
    // TODO(crbug.com/40281249): Support
    // AudioDeviceDescription::kLoopbackWithMuteDeviceId. Store the original
    // device name, i.e., AudioDeviceDescription::kLoopback*, and check it here
    // to determine if muting was requested for any of the streams, and mute the
    // default sink accordingly.
  }
  // Reapply muting with the new default monitor name
  if (has_muting_loopback_) {
    pulse::MuteAllSinksExcept(mainloop_, context_, default_monitor_name);
  }
}

}  // namespace media
