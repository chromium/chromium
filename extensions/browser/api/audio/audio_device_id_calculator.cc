// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_device_id_calculator.h"

#include "base/strings/string_number_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/api/audio/pref_names.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

AudioDeviceIdCalculator::AudioDeviceIdCalculator(
    content::BrowserContext* context)
    : context_(context) {}

AudioDeviceIdCalculator::~AudioDeviceIdCalculator() = default;

std::string AudioDeviceIdCalculator::GetStableDeviceId(
    uint64_t audio_service_stable_id) {
  if (!stable_id_map_loaded_) {
    LoadStableIdMap();
  }
  std::string audio_service_stable_id_str =
      base::NumberToString(audio_service_stable_id);
  const auto& it = stable_id_map_.find(audio_service_stable_id_str);
  if (it != stable_id_map_.end()) {
    return it->second;
  }
  return GenerateNewStableDeviceId(audio_service_stable_id_str);
}

void AudioDeviceIdCalculator::LoadStableIdMap() {
  DCHECK(!stable_id_map_loaded_);

  PrefService* pref_service =
      ExtensionsBrowserClient::Get()->GetPrefServiceForContext(context_);
  const base::Value::List& audio_service_stable_ids =
      pref_service->GetList(kAudioApiStableDeviceIds);
  const base::Value::List& ids_list = audio_service_stable_ids;
  for (size_t i = 0; i < ids_list.size(); ++i) {
    const std::string* audio_service_stable_id = ids_list[i].GetIfString();
    if (!audio_service_stable_id) {
      NOTREACHED_IN_MIGRATION() << "Non string stable device ID.";
      continue;
    }
    stable_id_map_[*audio_service_stable_id] = base::NumberToString(i);
  }
  stable_id_map_loaded_ = true;
}

std::string AudioDeviceIdCalculator::GenerateNewStableDeviceId(
    const std::string& audio_service_stable_id) {
  DCHECK(stable_id_map_loaded_);
  DCHECK_EQ(0u, stable_id_map_.count(audio_service_stable_id));

  ScopedListPrefUpdate update(
      ExtensionsBrowserClient::Get()->GetPrefServiceForContext(context_),
      kAudioApiStableDeviceIds);

  std::string api_stable_id = base::NumberToString(update->size());
  stable_id_map_[audio_service_stable_id] = api_stable_id;
  update->Append(audio_service_stable_id);
  return api_stable_id;
}

}  // namespace extensions
