// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_api.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/api/audio/audio_device_id_calculator.h"
#include "extensions/browser/api/audio/pref_names.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/audio.h"

namespace {
const char* const kLevelPropErrorMsg = "Could not set volume/gain properties";
}  // namespace

namespace extensions {

namespace audio = api::audio;

namespace {

std::unique_ptr<AudioDeviceIdCalculator> CreateIdCalculator(
    content::BrowserContext* context) {
  return std::make_unique<AudioDeviceIdCalculator>(context);
}

}  // namespace

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<AudioAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

void AudioAPI::RegisterUserPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kAudioApiStableDeviceIds);
}

// static
BrowserContextKeyedAPIFactory<AudioAPI>* AudioAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

AudioAPI::AudioAPI(content::BrowserContext* context)
    : browser_context_(context),
      stable_id_calculator_(CreateIdCalculator(context)),
      service_(AudioService::CreateInstance(stable_id_calculator_.get())) {
  audio_service_observation_.Observe(service_.get());
}

AudioAPI::~AudioAPI() = default;

AudioService* AudioAPI::GetService() const {
  return service_.get();
}

void AudioAPI::OnLevelChanged(const std::string& id, int level) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  audio::LevelChangedEvent raw_event;
  raw_event.device_id = id;
  raw_event.level = level;

  auto event_args = audio::OnLevelChanged::Create(raw_event);
  auto event = std::make_unique<Event>(events::AUDIO_ON_LEVEL_CHANGED,
                                       audio::OnLevelChanged::kEventName,
                                       std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

void AudioAPI::OnMuteChanged(bool is_input, bool is_muted) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  // Dispatch onMuteChanged event.
  audio::MuteChangedEvent raw_event;
  raw_event.stream_type =
      is_input ? audio::StreamType::kInput : audio::StreamType::kOutput;
  raw_event.is_muted = is_muted;
  auto event_args = audio::OnMuteChanged::Create(raw_event);
  auto event = std::make_unique<Event>(events::AUDIO_ON_MUTE_CHANGED,
                                       audio::OnMuteChanged::kEventName,
                                       std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

void AudioAPI::OnDevicesChanged(const DeviceInfoList& devices) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  auto args = audio::OnDeviceListChanged::Create(devices);
  auto event = std::make_unique<Event>(events::AUDIO_ON_DEVICES_CHANGED,
                                       audio::OnDeviceListChanged::kEventName,
                                       std::move(args));
  event_router->BroadcastEvent(std::move(event));
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetDevicesFunction::Run() {
  std::optional<audio::GetDevices::Params> params =
      audio::GetDevices::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  service->GetDevices(
      base::OptionalToPtr(params->filter),
      base::BindOnce(&AudioGetDevicesFunction::OnResponse, this));
  return RespondLater();
}

void AudioGetDevicesFunction::OnResponse(
    bool success,
    std::vector<api::audio::AudioDeviceInfo> devices) {
  const char* const kGetDevicesErrorMsg =
      "Error occurred when querying audio device information.";
  if (success) {
    Respond(ArgumentList(audio::GetDevices::Results::Create(devices)));
  } else {
    Respond(Error(kGetDevicesErrorMsg));
  }
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetActiveDevicesFunction::Run() {
  std::optional<audio::SetActiveDevices::Params> params =
      audio::SetActiveDevices::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  service->SetActiveDeviceLists(
      base::OptionalToPtr(params->ids.input),
      base::OptionalToPtr(params->ids.output),
      base::BindOnce(&AudioSetActiveDevicesFunction::OnResponse, this));
  return RespondLater();
}

void AudioSetActiveDevicesFunction::OnResponse(bool success) {
  const char* const kSetActiveDevicesErrorMsg = "Failed to set active devices.";
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kSetActiveDevicesErrorMsg));
  }
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetPropertiesFunction::Run() {
  std::optional<audio::SetProperties::Params> params =
      audio::SetProperties::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  bool level_set = !!params->properties.level;
  int level_value = level_set ? *params->properties.level : -1;

  if (level_set) {
    service->SetDeviceSoundLevel(
        params->id, level_value,
        base::BindOnce(&AudioSetPropertiesFunction::OnResponse, this));
    return RespondLater();
  }
  return RespondNow(Error(kLevelPropErrorMsg));
}

void AudioSetPropertiesFunction::OnResponse(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kLevelPropErrorMsg));
  }
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetMuteFunction::Run() {
  std::optional<audio::SetMute::Params> params =
      audio::SetMute::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  const bool is_input = (params->stream_type == audio::StreamType::kInput);

  service->SetMute(is_input, params->is_muted,
                   base::BindOnce(&AudioSetMuteFunction::OnResponse, this));
  return RespondLater();
}

void AudioSetMuteFunction::OnResponse(bool success) {
  const char* const kSetMuteErrorMsg = "Could not set mute state.";
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kSetMuteErrorMsg));
  }
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetMuteFunction::Run() {
  std::optional<audio::GetMute::Params> params =
      audio::GetMute::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);
  const bool is_input = (params->stream_type == audio::StreamType::kInput);

  service->GetMute(is_input,
                   base::BindOnce(&AudioGetMuteFunction::OnResponse, this));
  return RespondLater();
}

void AudioGetMuteFunction::OnResponse(bool success, bool is_muted) {
  const char* const kGetMuteErrorMsg = "Could not get mute state.";
  if (success) {
    Respond(ArgumentList(audio::GetMute::Results::Create(is_muted)));
  } else {
    Respond(Error(kGetMuteErrorMsg));
  }
}

}  // namespace extensions
