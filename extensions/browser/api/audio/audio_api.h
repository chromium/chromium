// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_AUDIO_API_H_
#define EXTENSIONS_BROWSER_API_AUDIO_AUDIO_API_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/audio/audio_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

class PrefRegistrySimple;

namespace extensions {

class AudioService;
class AudioDeviceIdCalculator;

class AudioAPI : public BrowserContextKeyedAPI, public AudioService::Observer {
 public:
  static void RegisterUserPrefs(PrefRegistrySimple* registry);

  explicit AudioAPI(content::BrowserContext* context);

  AudioAPI(const AudioAPI&) = delete;
  AudioAPI& operator=(const AudioAPI&) = delete;

  ~AudioAPI() override;

  AudioService* GetService() const;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<AudioAPI>* GetFactoryInstance();
  static const bool kServiceRedirectedInIncognito = true;

  // AudioService::Observer implementation.
  void OnLevelChanged(const std::string& id, int level) override;
  void OnMuteChanged(bool is_input, bool is_muted) override;
  void OnDevicesChanged(const DeviceInfoList& devices) override;

 private:
  friend class BrowserContextKeyedAPIFactory<AudioAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "AudioAPI";
  }

  const raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<AudioDeviceIdCalculator> stable_id_calculator_;
  std::unique_ptr<AudioService> service_;

  base::ScopedObservation<AudioService, AudioService::Observer>
      audio_service_observation_{this};
};

class AudioGetDevicesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.getDevices", AUDIO_GETDEVICES)

 protected:
  ~AudioGetDevicesFunction() override = default;
  ResponseAction Run() override;
  void OnResponse(bool success,
                  std::vector<api::audio::AudioDeviceInfo> devices);
};

class AudioSetActiveDevicesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setActiveDevices", AUDIO_SETACTIVEDEVICES)

 protected:
  ~AudioSetActiveDevicesFunction() override = default;
  ResponseAction Run() override;
  void OnResponse(bool success);
};

class AudioSetPropertiesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setProperties", AUDIO_SETPROPERTIES)

 protected:
  ~AudioSetPropertiesFunction() override = default;
  ResponseAction Run() override;
  void OnResponse(bool success);
};

class AudioSetMuteFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setMute", AUDIO_SETMUTE)

 protected:
  ~AudioSetMuteFunction() override = default;
  ResponseAction Run() override;
  void OnResponse(bool success);
};

class AudioGetMuteFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.getMute", AUDIO_GETMUTE)

 protected:
  ~AudioGetMuteFunction() override = default;
  ResponseAction Run() override;
  void OnResponse(bool success, bool is_muted);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_AUDIO_API_H_
