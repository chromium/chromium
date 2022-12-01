// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_AUDIO_DEVICE_ID_CALCULATOR_H_
#define EXTENSIONS_BROWSER_API_AUDIO_AUDIO_DEVICE_ID_CALCULATOR_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Helper class used to translate stable device IDs provided by audio service
// to stable ID values exposable to apps using chrome.audio API.
// Problem with stable device IDs provided by audio service is that they can be
// globally unique for audio devices and thus can be used by apps to identify a
// particular device. |AudioDeviceCalculator| provides stable mapping (saved in
// user prefs) from potentially globally unique ID to an ordinal ID (devices are
// assigned IDs incrementally, as they are added to the mapping) scoped to a
// browser context.
class AudioDeviceIdCalculator {
 public:
  explicit AudioDeviceIdCalculator(content::BrowserContext* context);

  AudioDeviceIdCalculator(const AudioDeviceIdCalculator&) = delete;
  AudioDeviceIdCalculator& operator=(const AudioDeviceIdCalculator&) = delete;

  virtual ~AudioDeviceIdCalculator();

  // Gets audio API stable device ID for the audio device whose stable device ID
  // equals |audio_service_stable_id|.
  std::string GetStableDeviceId(uint64_t audio_service_stable_id);

 private:
  // Loads stable ID mapping saved in user prefs.
  // Stable device ID map is saved in prefs as list of stable IDs provided by
  // audio service - stable ID at index i in the list is mapped to the ID "i".
  void LoadStableIdMap();

  // Generates and persists audio API stable device ID for the device whose
  // audio service stable device ID equals |audio_service_stable_id|.
  // Returns the generated ID.
  std::string GenerateNewStableDeviceId(
      const std::string& audio_service_stable_id);

  raw_ptr<content::BrowserContext, DanglingUntriaged> context_;

  // Maps a stable device ID as exposed by audio service to the associated
  // stable device ID that should be exposed to apps via chrome.audio API.
  std::map<std::string, std::string> stable_id_map_;
  bool stable_id_map_loaded_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_AUDIO_DEVICE_ID_CALCULATOR_H_
