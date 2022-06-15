// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/api/audio/audio_device_id_calculator.h"
#include "extensions/browser/api/audio/pref_names.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/audio.h"

namespace {
const char* const kGetDevicesErrorMsg =
    "Error occurred when querying audio device information.";
const char* const kSetActiveDevicesErrorMsg = "Failed to set active devices.";
const char* const kLevelPropErrorMsg = "Could not set volume/gain properties";
const char* const kSetMuteErrorMsg = "Could not set mute state.";
const char* const kGetMuteErrorMsg = "Could not get mute state.";
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

AudioAPI::~AudioAPI() {}

AudioService* AudioAPI::GetService() const {
  return service_.get();
}

void AudioAPI::OnLevelChanged(const std::string& id, int level) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

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
  if (!event_router)
    return;

  // Dispatch onMuteChanged event.
  audio::MuteChangedEvent raw_event;
  raw_event.stream_type =
      is_input ? audio::STREAM_TYPE_INPUT : audio::STREAM_TYPE_OUTPUT;
  raw_event.is_muted = is_muted;
  auto event_args = audio::OnMuteChanged::Create(raw_event);
  auto event = std::make_unique<Event>(events::AUDIO_ON_MUTE_CHANGED,
                                       audio::OnMuteChanged::kEventName,
                                       std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

void AudioAPI::OnDevicesChanged(const DeviceInfoList& devices) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  auto args = audio::OnDeviceListChanged::Create(devices);
  auto event = std::make_unique<Event>(events::AUDIO_ON_DEVICES_CHANGED,
                                       audio::OnDeviceListChanged::kEventName,
                                       std::move(args));
  event_router->BroadcastEvent(std::move(event));
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetDevicesFunction::Run() {
  std::unique_ptr<audio::GetDevices::Params> params(
      audio::GetDevices::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (service->GetDevices(
          params->filter.get(),
          base::BindOnce(&AudioGetDevicesFunction::OnResponse, this))) {
    return RespondLater();
  }
  return RespondNow(Error(kGetDevicesErrorMsg));

#else
  std::vector<api::audio::AudioDeviceInfo> devices;
  if (!service->GetDevices(params->filter.get(), &devices)) {
    return RespondNow(Error(kGetDevicesErrorMsg));
  }

  return RespondNow(ArgumentList(audio::GetDevices::Results::Create(devices)));
#endif
}

void AudioGetDevicesFunction::OnResponse(
    bool success,
    std::vector<api::audio::AudioDeviceInfo> devices) {
  if (success) {
    Respond(ArgumentList(audio::GetDevices::Results::Create(devices)));
  } else {
    Respond(Error(kGetDevicesErrorMsg));
  }
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetActiveDevicesFunction::Run() {
  std::unique_ptr<audio::SetActiveDevices::Params> params(
      audio::SetActiveDevices::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (service->SetActiveDeviceLists(
          params->ids.input.get(), params->ids.output.get(),
          base::BindOnce(&AudioSetActiveDevicesFunction::OnResponse, this))) {
    return RespondLater();
  }
#else
  if (service->SetActiveDeviceLists(params->ids.input.get(),
                                    params->ids.output.get())) {
    return RespondNow(NoArguments());
  }
#endif
  return RespondNow(Error(kSetActiveDevicesErrorMsg));
}

void AudioSetActiveDevicesFunction::OnResponse(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kSetActiveDevicesErrorMsg));
  }
}

///////////////////////////////////////////////////////////////////////////////
ExtensionFunction::ResponseAction AudioSetPropertiesFunction::Run() {
  std::unique_ptr<audio::SetProperties::Params> params(
      audio::SetProperties::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  bool level_set = !!params->properties.level;
  int level_value = level_set ? *params->properties.level : -1;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (level_set &&
      service->SetDeviceSoundLevel(
          params->id, level_value,
          base::BindOnce(&AudioSetPropertiesFunction::OnResponse, this))) {
    return RespondLater();
  }
#else
  if (level_set && service->SetDeviceSoundLevel(params->id, level_value)) {
    return RespondNow(NoArguments());
  }
#endif
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
  std::unique_ptr<audio::SetMute::Params> params(
      audio::SetMute::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  const bool is_input = (params->stream_type == audio::STREAM_TYPE_INPUT);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (service->SetMute(
          is_input, params->is_muted,
          base::BindOnce(&AudioSetMuteFunction::OnResponse, this))) {
    return RespondLater();
  }
  return RespondNow(Error(kSetMuteErrorMsg));
#else
  if (!service->SetMute(is_input, params->is_muted)) {
    return RespondNow(Error(kSetMuteErrorMsg));
  }
  return RespondNow(NoArguments());
#endif
}

void AudioSetMuteFunction::OnResponse(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kSetMuteErrorMsg));
  }
}

///////////////////////////////////////////////////////////////////////////////
ExtensionFunction::ResponseAction AudioGetMuteFunction::Run() {
  std::unique_ptr<audio::GetMute::Params> params(
      audio::GetMute::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);
  const bool is_input = (params->stream_type == audio::STREAM_TYPE_INPUT);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (service->GetMute(
          is_input, base::BindOnce(&AudioGetMuteFunction::OnResponse, this))) {
    return RespondLater();
  }
  return RespondNow(Error(kGetMuteErrorMsg));
#else
  bool value = false;
  if (!service->GetMute(is_input, &value)) {
    return RespondNow(Error(kGetMuteErrorMsg));
  }
  return RespondNow(ArgumentList(audio::GetMute::Results::Create(value)));
#endif
}

void AudioGetMuteFunction::OnResponse(bool success, bool is_muted) {
  if (success) {
    Respond(ArgumentList(audio::GetMute::Results::Create(is_muted)));
  } else {
    Respond(Error(kGetMuteErrorMsg));
  }
}

}  // namespace extensions
