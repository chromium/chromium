// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_api.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/api/audio/audio_device_id_calculator.h"
#include "extensions/browser/api/audio/pref_names.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/audio.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {

namespace audio = api::audio;

namespace {

std::unique_ptr<AudioDeviceIdCalculator> CreateIdCalculator(
    content::BrowserContext* context) {
  return std::make_unique<AudioDeviceIdCalculator>(context);
}

// Checks if an extension is whitelisted to use deprecated version of audio API.
// TODO(tbarzic): Retire this whitelist and remove the deprecated API methods,
//     properties and events. This is currently targeted for M-60
//     (http://crbug.com/697279).
bool CanUseDeprecatedAudioApi(const Extension* extension) {
  if (!extension)
    return false;

  const Feature* allow_deprecated_audio_api_feature =
      FeatureProvider::GetBehaviorFeatures()->GetFeature(
          behavior_feature::kAllowDeprecatedAudioApi);
  if (!allow_deprecated_audio_api_feature)
    return false;

  return allow_deprecated_audio_api_feature->IsAvailableToExtension(extension)
      .is_available();
}

bool CanReceiveDeprecatedAudioEvent(content::BrowserContext* browser_context,
                                    Feature::Context target_context,
                                    const Extension* extension,
                                    Event* event,
                                    const base::DictionaryValue* filter) {
  return CanUseDeprecatedAudioApi(extension);
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
      service_(AudioService::CreateInstance(stable_id_calculator_.get())),
      audio_service_observer_(this) {
  audio_service_observer_.Add(service_.get());
}

AudioAPI::~AudioAPI() {}

AudioService* AudioAPI::GetService() const {
  return service_.get();
}

void AudioAPI::OnDeviceChanged() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  std::unique_ptr<Event> event(new Event(
      events::AUDIO_ON_DEVICE_CHANGED, audio::OnDeviceChanged::kEventName,
      std::unique_ptr<base::ListValue>(new base::ListValue())));
  event->will_dispatch_callback =
      base::BindRepeating(&CanReceiveDeprecatedAudioEvent);
  event_router->BroadcastEvent(std::move(event));
}

void AudioAPI::OnLevelChanged(const std::string& id, int level) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  audio::LevelChangedEvent raw_event;
  raw_event.device_id = id;
  raw_event.level = level;

  std::unique_ptr<base::ListValue> event_args =
      audio::OnLevelChanged::Create(raw_event);
  std::unique_ptr<Event> event(new Event(events::AUDIO_ON_LEVEL_CHANGED,
                                         audio::OnLevelChanged::kEventName,
                                         std::move(event_args)));
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
  std::unique_ptr<base::ListValue> event_args =
      audio::OnMuteChanged::Create(raw_event);
  std::unique_ptr<Event> event(new Event(events::AUDIO_ON_MUTE_CHANGED,
                                         audio::OnMuteChanged::kEventName,
                                         std::move(event_args)));
  event_router->BroadcastEvent(std::move(event));
}

void AudioAPI::OnDevicesChanged(const DeviceInfoList& devices) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  std::unique_ptr<base::ListValue> args =
      audio::OnDeviceListChanged::Create(devices);
  std::unique_ptr<Event> event(new Event(events::AUDIO_ON_DEVICES_CHANGED,
                                         audio::OnDeviceListChanged::kEventName,
                                         std::move(args)));
  event_router->BroadcastEvent(std::move(event));
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetInfoFunction::Run() {
  if (!CanUseDeprecatedAudioApi(extension())) {
    return RespondNow(
        Error("audio.getInfo is deprecated, use audio.getDevices instead."));
  }

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);
  OutputInfo output_info;
  InputInfo input_info;
  if (!service->GetInfo(&output_info, &input_info)) {
    return RespondNow(
        Error("Error occurred when querying audio device information."));
  }

  return RespondNow(
      ArgumentList(audio::GetInfo::Results::Create(output_info, input_info)));
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetDevicesFunction::Run() {
  std::unique_ptr<audio::GetDevices::Params> params(
      audio::GetDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  std::vector<api::audio::AudioDeviceInfo> devices;
  if (!service->GetDevices(params->filter.get(), &devices)) {
    return RespondNow(
        Error("Error occurred when querying audio device information."));
  }

  return RespondNow(ArgumentList(audio::GetDevices::Results::Create(devices)));
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetActiveDevicesFunction::Run() {
  std::unique_ptr<audio::SetActiveDevices::Params> params(
      audio::SetActiveDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  if (params->ids.as_device_id_lists) {
    if (!service->SetActiveDeviceLists(
            params->ids.as_device_id_lists->input,
            params->ids.as_device_id_lists->output)) {
      return RespondNow(Error("Failed to set active devices."));
    }
  } else if (params->ids.as_strings) {
    if (!CanUseDeprecatedAudioApi(extension())) {
      return RespondNow(
          Error("String list |ids| is deprecated, use DeviceIdLists type."));
    }
    service->SetActiveDevices(*params->ids.as_strings);
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetPropertiesFunction::Run() {
  std::unique_ptr<audio::SetProperties::Params> params(
      audio::SetProperties::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  if (!CanUseDeprecatedAudioApi(extension())) {
    if (params->properties.volume)
      return RespondNow(Error("|volume| property is deprecated, use |level|."));

    if (params->properties.gain)
      return RespondNow(Error("|gain| property is deprecated, use |level|."));

    if (params->properties.is_muted) {
      return RespondNow(
          Error("|isMuted| property is deprecated, use |audio.setMute|."));
    }
  }

  bool level_set = !!params->properties.level;
  int level_value = level_set ? *params->properties.level : -1;

  int volume_value = params->properties.volume.get() ?
      *params->properties.volume : -1;

  int gain_value = params->properties.gain.get() ?
      *params->properties.gain : -1;

  // |volume_value| and |gain_value| are deprecated in favor of |level_value|;
  // they are kept around only to ensure backward-compatibility and should be
  // ignored if |level_value| is set.
  if (!service->SetDeviceSoundLevel(params->id,
                                    level_set ? level_value : volume_value,
                                    level_set ? level_value : gain_value))
    return RespondNow(Error("Could not set volume/gain properties"));

  if (params->properties.is_muted.get() &&
      !service->SetMuteForDevice(params->id, *params->properties.is_muted)) {
    return RespondNow(Error("Could not set mute property."));
  }

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetMuteFunction::Run() {
  std::unique_ptr<audio::SetMute::Params> params(
      audio::SetMute::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  if (!service->SetMute(params->stream_type == audio::STREAM_TYPE_INPUT,
                        params->is_muted)) {
    return RespondNow(Error("Could not set mute state."));
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetMuteFunction::Run() {
  std::unique_ptr<audio::GetMute::Params> params(
      audio::GetMute::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  bool value = false;
  if (!service->GetMute(params->stream_type == audio::STREAM_TYPE_INPUT,
                        &value)) {
    return RespondNow(Error("Could not get mute state."));
  }
  return RespondNow(ArgumentList(audio::GetMute::Results::Create(value)));
}

}  // namespace extensions
